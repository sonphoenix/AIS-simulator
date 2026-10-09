#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ais_sim/wave.hpp>
#include <shaderClass.hpp>

namespace ais_sim {

    // One level of detail of the ocean mesh: a grid made of every `factor`-th
    // point of the master wave. All LODs read the SAME master wave, so they
    // show the same pattern and tiles line up across LOD borders.
    class OceanLOD {
    public:
        OceanLOD(int masterN, int factor);
        ~OceanLOD();

        OceanLOD(const OceanLOD&) = delete;
        OceanLOD& operator=(const OceanLOD&) = delete;

        // Copies the master wave's current heights/normals into this LOD's GPU buffer.
        void update(const Wave& master);

        // Draws one tile of this LOD at the position given by tileModel.
        void draw(Shader& shader, const glm::mat4& tileModel) const;

        // Deletes the VAO, VBO and EBO now. Safe to call more than once, and the
        // destructor calls it too. Don't update() or draw() after this.
        void release();

    private:
        int masterN_;
        int factor_;
        std::vector<unsigned int> indices_;
        std::vector<float> vertexBuffer_;
        GLuint VAO_ = 0, VBO_ = 0, EBO_ = 0;
        float heightMin_ = 0.0f, heightMax_ = 0.0f;
    };

    // The whole ocean: one master FFT wave, three LODs, one shader.
    class Ocean {
    public:
        Ocean();   // needs an OpenGL context (builds buffers and compiles the shader)

        Ocean(const Ocean&) = delete;
        Ocean& operator=(const Ocean&) = delete;

        // Runs the FFT for this time and refreshes all three LOD buffers.
        void update(float time);

        // Draws the grid of tiles around the camera.
        void draw(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& cameraPos, const glm::vec3& lightPos);

        // Map mode: covers a whole rectangle (centered on the origin) with
        // low-detail tiles anchored to the world, regardless of the camera.
        void drawMap(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& cameraPos, const glm::vec3& lightPos,
            float mapSizeX, float mapSizeZ);

        void drawPlane(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& cameraPos, const glm::vec3& lightPos,
            float centerX, float centerZ, float halfSize, float seaLevel);

        void drawWorld(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& cameraPos, const glm::vec3& lightPos,
            float seaLevel, int radius,
            GLuint terrainHeightTex, float terrainMetersPerTexel);

        // Frees the GPU buffers of all three LODs. Call it while the OpenGL
        // context still exists (before glfwTerminate). Don't draw after this.
        void release();

    private:
        void setupShader(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& cameraPos, const glm::vec3& lightPos);

        static constexpr float kPatchSize = 1000.0f;
        static constexpr int   kMasterN = 256;
        static constexpr float kYOffset = 10.0f;
        static constexpr int   kOuterRadius = 3;

        void buildPlane();
        GLuint planeVAO_ = 0, planeVBO_ = 0, planeEBO_ = 0;

        Wave masterWave_;
        OceanLOD high_;
        OceanLOD mid_;
        OceanLOD low_;
        Shader shader_;
    };

} // namespace ais_sim