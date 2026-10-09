#include <ais_sim/world_terrain.hpp>
#include <stb_image.h>
#include <cmath>
#include <iostream>
#include <vector>

namespace ais_sim {

    // Loads a repeating ground picture (grass, rock...) with mipmaps.
    static GLuint loadGroundTexture(const std::string& path) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        int w, h, channels;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);   // force RGBA
        if (data) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load texture: " << path << "\n";
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return tex;
    }

    WorldTerrain::WorldTerrain(const HeightGrid& grid, double centerLatDeg, int zoom)
        : shader_("shaders/world_terrain.vs", "shaders/world_terrain.fs"),
        gridW_(grid.width),
        gridH_(grid.height)
    {
        const double kPi = 3.14159265358979323846;

        // Ground size of one pixel of a Web Mercator tile at this latitude and zoom.
        // (156543 m is the size of one pixel at zoom 0 on the equator.)
        metersPerTexel_ = (float)(156543.03392 * std::cos(centerLatDeg * kPi / 180.0)
            / std::pow(2.0, zoom));
        halfSize_ = (gridW_ / 2) * metersPerTexel_;

        std::cout << "world terrain: " << gridW_ << "x" << gridH_ << " heights, "
            << metersPerTexel_ << " m per pixel, about "
            << (gridW_ * metersPerTexel_) / 1000.0f << " km wide\n";

        uploadHeights(grid);
        buildMesh();

        grassTex_ = loadGroundTexture("textures/terrain/grass.png");
        rockTex_ = loadGroundTexture("textures/terrain/rock.png");
        sandTex_ = loadGroundTexture("textures/terrain/sand.png");
        snowTex_ = loadGroundTexture("textures/terrain/snow.png");
    }

    WorldTerrain::~WorldTerrain() {
        release();
    }

    void WorldTerrain::release() {
        if (heightTex_) { glDeleteTextures(1, &heightTex_); heightTex_ = 0; }
        if (grassTex_) { glDeleteTextures(1, &grassTex_);  grassTex_ = 0; }
        if (rockTex_) { glDeleteTextures(1, &rockTex_);   rockTex_ = 0; }
        if (sandTex_) { glDeleteTextures(1, &sandTex_);   sandTex_ = 0; }
        if (snowTex_) { glDeleteTextures(1, &snowTex_);   snowTex_ = 0; }
        if (VAO_) { glDeleteVertexArrays(1, &VAO_); VAO_ = 0; }
        if (VBO_) { glDeleteBuffers(1, &VBO_);      VBO_ = 0; }
        if (EBO_) { glDeleteBuffers(1, &EBO_);      EBO_ = 0; }
    }

    // Copies the list of heights (meters) to the GPU as a one-channel float texture.
    void WorldTerrain::uploadHeights(const HeightGrid& grid) {
        glGenTextures(1, &heightTex_);
        glBindTexture(GL_TEXTURE_2D, heightTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, gridW_, gridH_, 0,
            GL_RED, GL_FLOAT, grid.meters.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // One vertex per height pixel. Vertex (col, row) sits at x = (col - center) * metersPerTexel,
    // z = (row - center) * metersPerTexel, so the middle pixel is the world origin.
    // Row 0 is north, which is -z (same as your world map).
    void WorldTerrain::buildMesh() {
        std::vector<float> vertices;
        vertices.reserve((size_t)gridW_ * gridH_ * 5);

        const float cx = (float)(gridW_ / 2);
        const float cz = (float)(gridH_ / 2);

        for (int row = 0; row < gridH_; row++) {
            for (int col = 0; col < gridW_; col++) {
                vertices.push_back((col - cx) * metersPerTexel_);      // x
                vertices.push_back(0.0f);                              // y (the shader sets it)
                vertices.push_back((row - cz) * metersPerTexel_);      // z
                vertices.push_back((float)col / (float)(gridW_ - 1));  // u: 0..1
                vertices.push_back((float)row / (float)(gridH_ - 1));  // v: 0..1
            }
        }

        std::vector<unsigned int> indices;
        indices.reserve((size_t)(gridW_ - 1) * (gridH_ - 1) * 6);
        for (int row = 0; row < gridH_ - 1; row++) {
            for (int col = 0; col < gridW_ - 1; col++) {
                unsigned int topLeft = row * gridW_ + col;
                unsigned int topRight = topLeft + 1;
                unsigned int bottomLeft = (row + 1) * gridW_ + col;
                unsigned int bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }
        indexCount_ = (int)indices.size();

        glGenVertexArrays(1, &VAO_);
        glGenBuffers(1, &VBO_);
        glGenBuffers(1, &EBO_);

        glBindVertexArray(VAO_);

        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);   // aPos
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);   // aTexCoord
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void WorldTerrain::draw(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& sunDir, const glm::vec3& lightColor)
    {
        shader_.use();

        shader_.setMat4("model", glm::mat4(1.0f));
        shader_.setMat4("view", view);
        shader_.setMat4("projection", projection);

        shader_.setFloat("metersPerTexel", metersPerTexel_);
        shader_.setFloat("heightScale", heightScale_);
        shader_.setFloat("tileScale", 75.0f);          // meters per repeat of the ground pictures
        shader_.setVec3("sunDir", sunDir);
        shader_.setVec3("lightColor", lightColor);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, heightTex_);
        shader_.setInt("heightmap", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, grassTex_);
        shader_.setInt("grassTex", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, rockTex_);
        shader_.setInt("rockTex", 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, sandTex_);
        shader_.setInt("sandTex", 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, snowTex_);
        shader_.setInt("snowTex", 4);

        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }

} // namespace ais_sim