#include <ais_sim/tile_fetcher.hpp>
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace ais_sim {

    namespace fs = std::filesystem;

    static constexpr double kPi = 3.14159265358979323846;

    TileFetcher::TileFetcher(const std::string& cacheDir)
        : cacheDir_(cacheDir)
    {
        // https needs certificates. "SYSTEM" = use the OS certificate store.
        // If your AisClient sets TLS options differently, copy what it does here.
        ix::SocketTLSOptions tls;
        tls.caFile = "SYSTEM";
        http_.setTLSOptions(tls);
    }

    // ============================================================
    // Tile math
    // ============================================================

    TileCoord TileFetcher::latLonToTile(double lat, double lon, int zoom) {
        double n = std::pow(2.0, zoom);                  // tiles across the whole world
        double latRad = lat * kPi / 180.0;               // the formula wants radians
        TileCoord t;
        t.x = (int)std::floor((lon + 180.0) / 360.0 * n);
        t.y = (int)std::floor((1.0 - std::asinh(std::tan(latRad)) / kPi) / 2.0 * n);
        return t;
    }

    void TileFetcher::tileToLatLon(int x, int y, int zoom, double& lat, double& lon) {
        double n = std::pow(2.0, zoom);
        lon = x / n * 360.0 - 180.0;
        double latRad = std::atan(std::sinh(kPi * (1.0 - 2.0 * y / n)));
        lat = latRad * 180.0 / kPi;
    }

    // ============================================================
    // Download
    // ============================================================

    std::string TileFetcher::tilePath(int zoom, int x, int y) const {
        // e.g. cache/terrain/10/163/396.png
        return (fs::path(cacheDir_) / std::to_string(zoom) / std::to_string(x)
            / (std::to_string(y) + ".png")).string();
    }

    std::string TileFetcher::downloadTile(int zoom, int x, int y) {
        std::string path = tilePath(zoom, x, y);

        if (fs::exists(path)) {
            std::cout << "tile already on disk: " << path << "\n";
            return path;
        }

        std::string url = "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/"
            + std::to_string(zoom) + "/" + std::to_string(x) + "/" + std::to_string(y) + ".png";
        std::cout << "downloading " << url << "\n";

        ix::HttpRequestArgsPtr args = http_.createRequest();
        args->connectTimeout = 10;
        args->transferTimeout = 30;

        ix::HttpResponsePtr res = http_.get(url, args);
        if (!res || res->statusCode != 200) {
            std::cerr << "download failed. status: " << (res ? res->statusCode : 0)
                << "  error: " << (res ? res->errorMsg : "no response") << "\n";
            return "";
        }

        fs::create_directories(fs::path(path).parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(res->body.data(), (std::streamsize)res->body.size());
        std::cout << "saved " << res->body.size() << " bytes to " << path << "\n";
        return path;
    }

    // ============================================================
    // Decode
    // ============================================================

    bool TileFetcher::decodeTile(const std::string& pngPath, std::vector<float>& meters) {
        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(false);   // tile row 0 must stay the north row
        unsigned char* px = stbi_load(pngPath.c_str(), &w, &h, &channels, 3);   // 3 = give me R, G, B
        if (!px) {
            std::cerr << "could not read " << pngPath << "\n";
            return false;
        }
        if (w != 256 || h != 256) {
            std::cerr << "unexpected tile size " << w << "x" << h << "\n";
            stbi_image_free(px);
            return false;
        }

        meters.resize(w * h);
        for (int i = 0; i < w * h; i++) {
            float R = px[i * 3 + 0];
            float G = px[i * 3 + 1];
            float B = px[i * 3 + 2];
            meters[i] = R * 256.0f + G + B / 256.0f - 32768.0f;   // the Terrarium formula
        }
        stbi_image_free(px);
        return true;
    }

    // ============================================================
    // 3 x 3 block of tiles
    // ============================================================

    HeightGrid TileFetcher::fetchBlock3x3(int zoom, TileCoord center) {
        const int N = 1 << zoom;     // tiles across the whole world at this zoom
        const int T = 256;           // pixels along one side of a tile

        HeightGrid grid;
        grid.width = 3 * T;
        grid.height = 3 * T;
        grid.meters.resize((size_t)grid.width * grid.height);   // starts filled with zeros

        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                int tx = center.x + (col - 1);        // -1, 0, +1 around the middle
                int ty = center.y + (row - 1);

                if (tx < 0)  tx += N;                 // east-west wraps around the world
                if (tx >= N) tx -= N;
                if (ty < 0 || ty >= N) continue;      // no tiles beyond the top/bottom: leave zeros

                std::string path = downloadTile(zoom, tx, ty);
                std::vector<float> tile;
                if (path.empty() || !decodeTile(path, tile)) {
                    std::cerr << "block failed at tile " << tx << ", " << ty << "\n";
                    return HeightGrid{};
                }

                // copy the tile's rows into the big array
                for (int r = 0; r < T; r++) {
                    for (int c = 0; c < T; c++) {
                        int bigX = col * T + c;               // add the tile's offset
                        int bigY = row * T + r;
                        grid.meters[(size_t)bigY * grid.width + bigX] = tile[r * T + c];
                    }
                }
            }
        }
        return grid;
    }

} // namespace ais_sim