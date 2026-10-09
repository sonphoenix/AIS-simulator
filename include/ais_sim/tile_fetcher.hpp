#pragma once
#include <ixwebsocket/IXHttpClient.h>
#include <string>
#include<vector>

namespace ais_sim {

    // Address of one map tile at a given zoom: x = column, y = row (0 = north edge).
    struct TileCoord {
        int x = 0;
        int y = 0;
    };

    // A rectangle of heights in meters. Row 0 is the north edge.
    struct HeightGrid {
        int width = 0;
        int height = 0;
        std::vector<float> meters;     // width * height values, row after row

        float at(int x, int y) const { return meters[(size_t)y * width + x]; }
    };

    // Tile math + downloading elevation tiles (AWS "Terrarium" PNGs).
    class TileFetcher {
    public:
        explicit TileFetcher(const std::string& cacheDir = "cache/terrain");

        // ---- tile math (no network, no files) ----

        // Which tile contains this lat/lon (degrees)?
        static TileCoord latLonToTile(double lat, double lon, int zoom);

        // Lat/lon of the top-left (north-west) corner of a tile.
        static void tileToLatLon(int x, int y, int zoom, double& lat, double& lon);


        std::string downloadTile(int zoom, int x, int y);

        bool decodeTile(const std::string& pngPath, std::vector<float>& meters);

        HeightGrid fetchBlock3x3(int zoom, TileCoord center);

    private:
        std::string tilePath(int zoom, int x, int y) const;

        std::string cacheDir_;
        ix::HttpClient http_;
    };

} // namespace ais_sim