#include <ais_sim/ocean.hpp>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace ais_sim {



    static std::vector<unsigned int> buildIndices(int N, int M) {
        int stride = N + 1;
        std::vector<unsigned int> indices;
        indices.reserve(N * M * 6);
        for (int row = 0; row < M; row++) {
            for (int col = 0; col < N; col++) {
                unsigned int i0 = row * stride + col;
                unsigned int i1 = row * stride + col + 1;
                unsigned int i2 = (row + 1) * stride + col;
                unsigned int i3 = (row + 1) * stride + col + 1;
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i3);
                indices.push_back(i2);
            }
        }
        return indices;
    }

    // ============================================================
    // OceanLOD
    // ============================================================

    // masterN must be evenly divisible by factor
    OceanLOD::OceanLOD(int masterN, int factor)
        : masterN_(masterN), factor_(factor)
    {
        int lodN = masterN_ / factor_;
        indices_ = buildIndices(lodN, lodN);
        vertexBuffer_.resize((lodN + 1) * (lodN + 1) * 6);

        glGenVertexArrays(1, &VAO_);
        glGenBuffers(1, &VBO_);
        glGenBuffers(1, &EBO_);

        glBindVertexArray(VAO_);

        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer_.size() * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), indices_.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    OceanLOD::~OceanLOD() {
        release();
    }

    void OceanLOD::release() {
        // Each handle is set to 0 after deleting, so calling this twice
        // (or calling it and then letting the destructor run) is harmless.
        if (VAO_) { glDeleteVertexArrays(1, &VAO_); VAO_ = 0; }
        if (VBO_) { glDeleteBuffers(1, &VBO_);      VBO_ = 0; }
        if (EBO_) { glDeleteBuffers(1, &EBO_);      EBO_ = 0; }
    }

    void OceanLOD::update(const Wave& master) {
        int masterStride = masterN_ + 1;
        int lodN = masterN_ / factor_;
        int lodStride = lodN + 1;

        heightMin_ = 1e9f;
        heightMax_ = -1e9f;

        for (int row = 0; row <= lodN; row++) {
            for (int col = 0; col <= lodN; col++) {
                int masterIndex = (row * factor_) * masterStride + (col * factor_);
                int lodIndex = row * lodStride + col;

                const glm::vec3& pos = master.heightField[masterIndex];
                const glm::vec3& norm = master.normalField[masterIndex];

                heightMin_ = std::min(heightMin_, pos.y);
                heightMax_ = std::max(heightMax_, pos.y);

                vertexBuffer_[lodIndex * 6 + 0] = pos.x;
                vertexBuffer_[lodIndex * 6 + 1] = pos.y;
                vertexBuffer_[lodIndex * 6 + 2] = pos.z;
                vertexBuffer_[lodIndex * 6 + 3] = norm.x;
                vertexBuffer_[lodIndex * 6 + 4] = norm.y;
                vertexBuffer_[lodIndex * 6 + 5] = norm.z;
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, VBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexBuffer_.size() * sizeof(float), vertexBuffer_.data());
    }

    void OceanLOD::draw(Shader& shader, const glm::mat4& tileModel) const {
        shader.setMat4("model", tileModel);
        shader.setFloat("heightMin", heightMin_);
        shader.setFloat("heightMax", heightMax_);
        glBindVertexArray(VAO_);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices_.size(), GL_UNSIGNED_INT, 0);
    }

    // ============================================================
    // Ocean
    // ============================================================

    Ocean::Ocean()
        : masterWave_(kMasterN, kMasterN, kPatchSize, kPatchSize,
            glm::vec2(1.0f, 1.0f), 15.0f, 3e-7f, 1.0f),
        high_(kMasterN, 1),   // full res
        mid_(kMasterN, 2),    // every 2nd master point
        low_(kMasterN, 4),    // every 4th master point
        shader_("shaders/ocean.vs", "shaders/ocean.fs")
    {
        buildPlane();         // makes the flat sheet of water (4 corners, 2 triangles)
    }

    void Ocean::release() {
        high_.release();
        mid_.release();
        low_.release();
        if (planeVAO_) { glDeleteVertexArrays(1, &planeVAO_); planeVAO_ = 0; }
        if (planeVBO_) { glDeleteBuffers(1, &planeVBO_);      planeVBO_ = 0; }
        if (planeEBO_) { glDeleteBuffers(1, &planeEBO_);      planeEBO_ = 0; }
    }

    void Ocean::update(float time) {
        masterWave_.buildField(time);   // the ONLY FFT per frame
        high_.update(masterWave_);
        mid_.update(masterWave_);
        low_.update(masterWave_);
    }

    void Ocean::setupShader(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& cameraPos, const glm::vec3& lightPos)
    {
        shader_.use();
        shader_.setVec3("lightPos", lightPos);
        shader_.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
        shader_.setVec3("viewPos", cameraPos);
        shader_.setMat4("view", view);
        shader_.setMat4("projection", projection);
        shader_.setInt("useMask", 0);   // off by default; drawWorld turns it on
    }


    void Ocean::drawMap(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& cameraPos, const glm::vec3& lightPos,
        float mapSizeX, float mapSizeZ)
    {
        setupShader(view, projection, cameraPos, lightPos);

        int tilesX = (int)std::ceil(mapSizeX / kPatchSize);
        int tilesZ = (int)std::ceil(mapSizeZ / kPatchSize);
        float startX = -mapSizeX * 0.5f + kPatchSize * 0.5f;   // center of the first tile
        float startZ = -mapSizeZ * 0.5f + kPatchSize * 0.5f;

        for (int j = 0; j < tilesZ; j++) {
            for (int i = 0; i < tilesX; i++) {
                glm::mat4 tileModel = glm::translate(glm::mat4(1.0f), glm::vec3(
                    startX + i * kPatchSize,
                    kYOffset,
                    startZ + j * kPatchSize));
                low_.draw(shader_, tileModel);
            }
        }
    }

    void Ocean::draw(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& cameraPos, const glm::vec3& lightPos)
    {
        setupShader(view, projection, cameraPos, lightPos);

        // World-anchored: tile centers snap to multiples of kPatchSize, so the
        // waves stay put in the world while the camera moves (the grid only
        // jumps by whole tiles). To freeze the grid completely, use 0.0f for both.
        float centerTileX = std::round(cameraPos.x / kPatchSize);
        float centerTileZ = std::round(cameraPos.z / kPatchSize);

        for (int tz = -kOuterRadius; tz <= kOuterRadius; tz++) {
            for (int tx = -kOuterRadius; tx <= kOuterRadius; tx++) {
                int dist = std::max(std::abs(tx), std::abs(tz));

                glm::mat4 tileModel = glm::translate(glm::mat4(1.0f), glm::vec3(
                    (centerTileX + tx) * kPatchSize,
                    kYOffset,
                    (centerTileZ + tz) * kPatchSize));

                if (dist <= 1)       high_.draw(shader_, tileModel);
                else if (dist == 2)  mid_.draw(shader_, tileModel);
                else                 low_.draw(shader_, tileModel);
            }
        }
    }

    void Ocean::buildPlane() {
        // A square from -1 to +1 on the ground: position (3 floats) + normal (3 floats) per
        // corner, the same layout as the wave meshes, so the same shader can draw it.
        // The normal (0, 1, 0) means "this surface faces straight up".
        float v[] = {
            -1, 0, -1,   0, 1, 0,
             1, 0, -1,   0, 1, 0,
             1, 0,  1,   0, 1, 0,
            -1, 0,  1,   0, 1, 0,
        };
        unsigned int idx[] = { 0, 2, 1,  0, 3, 2 };    // two triangles

        glGenVertexArrays(1, &planeVAO_);
        glGenBuffers(1, &planeVBO_);
        glGenBuffers(1, &planeEBO_);
        glBindVertexArray(planeVAO_);
        glBindBuffer(GL_ARRAY_BUFFER, planeVBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        glBindVertexArray(0);
    }

    void Ocean::drawPlane(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& cameraPos, const glm::vec3& lightPos,
        float centerX, float centerZ, float halfSize, float seaLevel)
    {
        setupShader(view, projection, cameraPos, lightPos);

        // stretch the -1..+1 square to the wanted size, then move it to sea level
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(centerX, seaLevel, centerZ));
        model = glm::scale(model, glm::vec3(halfSize, 1.0f, halfSize));

        shader_.setMat4("model", model);
        shader_.setFloat("heightMin", -1.0f);   // the wave shader expects these two numbers
        shader_.setFloat("heightMax", 1.0f);

        glBindVertexArray(planeVAO_);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void Ocean::drawWorld(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& cameraPos, const glm::vec3& lightPos,
        float seaLevel, int radius,
        GLuint terrainHeightTex, float terrainMetersPerTexel)
    {
        setupShader(view, projection, cameraPos, lightPos);

        // Terrain mask: the fragment shader throws away water that lies over land.
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, terrainHeightTex);
        shader_.setInt("terrainHeight", 5);
        shader_.setFloat("terrainMPT", terrainMetersPerTexel);
        shader_.setFloat("seaLevel", seaLevel);
        shader_.setInt("useMask", 1);

        float centerTileX = std::round(cameraPos.x / kPatchSize);
        float centerTileZ = std::round(cameraPos.z / kPatchSize);
        int altBias = (int)(std::max(0.0f, cameraPos.y - seaLevel) / (kPatchSize * 2.0f));

        for (int tz = -radius; tz <= radius; tz++) {
            for (int tx = -radius; tx <= radius; tx++) {
                int dist = std::max(std::abs(tx), std::abs(tz)) + altBias;

                glm::mat4 tileModel = glm::translate(glm::mat4(1.0f), glm::vec3(
                    (centerTileX + tx) * kPatchSize,
                    seaLevel,
                    (centerTileZ + tz) * kPatchSize));

                if (dist <= 1)       high_.draw(shader_, tileModel);
                else if (dist == 2)  mid_.draw(shader_, tileModel);
                else                 low_.draw(shader_, tileModel);
            }
        }

        shader_.setInt("useMask", 0);
        glActiveTexture(GL_TEXTURE0);
    }

} // namespace ais_sim