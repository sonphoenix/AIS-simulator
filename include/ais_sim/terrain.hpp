#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>

#include "shaderClass.hpp"

namespace ais_sim {

    class Terrain {
    public:

        Terrain(const std::string& heightmapPath, int meshWidth, int meshHeight,
            float worldSizeX, float worldSizeZ, float maxElevationMeters,
            float lonMin, float lonMax, float latMin, float latMax);

        ~Terrain();



        void draw(
            const glm::mat4& view,
            const glm::mat4& projection,
            const glm::vec3& lightPos,
            const glm::vec3& lightColor,
            const glm::mat4& model
        );

        void loadTerrainTextures();

    private:
        void loadHeightmap(const std::string& path);
        void buildMesh();

        Shader shader_;
        GLuint VAO_ = 0, VBO_ = 0, EBO_ = 0;
        GLuint heightmapTexture_ = 0;
        GLuint grassTex_ = 0, rockTex_ = 0, sandTex_ = 0, snowTex_ = 0;

        int meshWidth_ = 0;
        int meshHeight_ = 0;
        float worldSizeX_ = 0.0f;
        float worldSizeZ_ = 0.0f;
        float maxElevationMeters_ = 0.0f;

        int heightmapWidth_ = 0;
        int heightmapHeight_ = 0;

        glm::vec2 uvMin_;   // add these two
        glm::vec2 uvMax_;

        int indexCount_ = 0;
    };

}