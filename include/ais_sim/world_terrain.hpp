#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <shaderClass.hpp>
#include <ais_sim/tile_fetcher.hpp>   // for HeightGrid

namespace ais_sim {

    // The terrain of one generated world: a flat grid of vertices, measured in
    // meters and centered on the origin, lifted by a height texture (in meters).
    // Needs an OpenGL context (it makes GPU buffers and compiles a shader).
    class WorldTerrain {
    public:
        // grid:         the stitched heights (e.g. 768 x 768, row 0 = north)
        // centerLatDeg: latitude of the middle of the block, to know how many
        //               meters one pixel covers
        // zoom:         the zoom level the tiles were downloaded at
        WorldTerrain(const HeightGrid& grid, double centerLatDeg, int zoom);
        ~WorldTerrain();

        WorldTerrain(const WorldTerrain&) = delete;
        WorldTerrain& operator=(const WorldTerrain&) = delete;

        void draw(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& sunDir, const glm::vec3& lightColor);

        // Frees the GPU objects now (safe to call twice; the destructor calls it too).
        void release();

        void setHeightScale(float s) { heightScale_ = s; }   // 1 = real, 2 = twice as tall
        float metersPerTexel() const { return metersPerTexel_; }
        float halfSizeMeters() const { return halfSize_; }   // distance from the origin to an edge
        GLuint heightTexture() const { return heightTex_; }

    private:
        void uploadHeights(const HeightGrid& grid);
        void buildMesh();

        // Declaration order = initialization order.
        Shader shader_;
        int gridW_, gridH_;
        float metersPerTexel_ = 1.0f;
        float halfSize_ = 0.0f;
        float heightScale_ = 1.0f;
        int indexCount_ = 0;

        GLuint heightTex_ = 0;
        GLuint VAO_ = 0, VBO_ = 0, EBO_ = 0;
        GLuint grassTex_ = 0, rockTex_ = 0, sandTex_ = 0, snowTex_ = 0;
    };

} // namespace ais_sim