#include "ais_sim/terrain.hpp"

#include <stb_image.h>

#include <iostream>
#include <vector>

namespace ais_sim {

    Terrain::Terrain(
        const std::string& heightmapPath,
        int meshWidth,
        int meshHeight,
        float worldSizeX,
        float worldSizeZ,
        float maxElevationMeters,
        float lonMin,
        float lonMax,
        float latMin,
        float latMax
    )
        : shader_(
            "shaders/terrain.vs",
            "shaders/terrain.fs"
        ),
        meshWidth_(meshWidth),
        meshHeight_(meshHeight),
        worldSizeX_(worldSizeX),
        worldSizeZ_(worldSizeZ),
        maxElevationMeters_(maxElevationMeters)
    {
        // Convert real lon/lat degrees into a 0-1 UV rectangle within the
        // world heightmap image (equirectangular: left edge = -180 lon,
        // top edge = +90 lat).
        uvMin_ = glm::vec2(
            (lonMin + 180.0f) / 360.0f,
            (90.0f - latMax) / 180.0f   // latitude flips: image row 0 = north pole
        );
        uvMax_ = glm::vec2(
            (lonMax + 180.0f) / 360.0f,
            (90.0f - latMin) / 180.0f
        );

        loadHeightmap(heightmapPath);
        std::cout << "mesh: " << meshWidth_ << "x" << meshHeight_
            << ", image: " << heightmapWidth_ << "x" << heightmapHeight_ << "\n";
        loadTerrainTextures();
        buildMesh();
    }


    Terrain::~Terrain()
    {
        if (heightmapTexture_ != 0)
            glDeleteTextures(1, &heightmapTexture_);

        if (VAO_ != 0)
            glDeleteVertexArrays(1, &VAO_);

        if (VBO_ != 0)
            glDeleteBuffers(1, &VBO_);

        if (EBO_ != 0)
            glDeleteBuffers(1, &EBO_);
    }


    // ============================================================
    // LOAD HEIGHTMAP
    // ============================================================

    void Terrain::loadHeightmap(
        const std::string& path
    )
    {

        int channels = 0;

        unsigned char* data = stbi_load(
            path.c_str(),
            &heightmapWidth_,
            &heightmapHeight_,
            &channels,
            1
        );


        if (!data)
        {
            std::cerr
                << "Failed to load heightmap: "
                << path
                << "\n";

            return;
        }

        std::cout
            << "Heightmap loaded: "
            << heightmapWidth_
            << "x"
            << heightmapHeight_
            << "\n";

        std::vector<unsigned char> cleaned(data, data + (heightmapWidth_ * heightmapHeight_));

        const int threshold = 60; // how big a jump counts as "spike" (0-255 scale)

        for (int y = 1; y < heightmapHeight_ - 1; y++) {
            for (int x = 1; x < heightmapWidth_ - 1; x++) {
                int idx = y * heightmapWidth_ + x;

                int neighborSum =
                    data[idx - 1] + data[idx + 1] +
                    data[idx - heightmapWidth_] + data[idx + heightmapWidth_];
                int neighborAvg = neighborSum / 4;

                if (std::abs((int)data[idx] - neighborAvg) > threshold) {
                    cleaned[idx] = (unsigned char)neighborAvg;
                }
            }
        }




        // --------------------------------------------------------
        // Create OpenGL texture
        // --------------------------------------------------------

        glGenTextures(
            1,
            &heightmapTexture_
        );

        glBindTexture(
            GL_TEXTURE_2D,
            heightmapTexture_
        );


        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, heightmapWidth_, heightmapHeight_,
            0, GL_RED, GL_UNSIGNED_BYTE, cleaned.data());


        // --------------------------------------------------------
        // Filtering
        // --------------------------------------------------------

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            GL_LINEAR
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            GL_LINEAR
        );


        // --------------------------------------------------------
        // Don't wrap around at the edges.
        // --------------------------------------------------------

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_CLAMP_TO_EDGE
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_CLAMP_TO_EDGE
        );


        glBindTexture(
            GL_TEXTURE_2D,
            0
        );


        stbi_image_free(data);
    }


    // ============================================================
    // BUILD FLAT GRID
    // ============================================================

    void Terrain::buildMesh()
    {
        std::vector<float> vertices;

        vertices.reserve(
            meshWidth_ *
            meshHeight_ *
            5
        );


        const float halfX =
            worldSizeX_ * 0.5f;

        const float halfZ =
            worldSizeZ_ * 0.5f;


        // --------------------------------------------------------
        // Vertices
        //
        // Position + texture coordinate
        //
        // The Y coordinate is intentionally 0.
        //
        // The vertex shader will replace it with the heightmap
        // elevation.
        // --------------------------------------------------------

        for (int row = 0;
            row < meshHeight_;
            ++row)
        {
            for (int col = 0;
                col < meshWidth_;
                ++col)
            {
                float u =
                    static_cast<float>(col) /
                    static_cast<float>(meshWidth_ - 1);

                float v =
                    static_cast<float>(row) /
                    static_cast<float>(meshHeight_ - 1);


                float x =
                    -halfX +
                    u * worldSizeX_;

                float z =
                    -halfZ +
                    v * worldSizeZ_;


                // Position
                vertices.push_back(x);
                vertices.push_back(0.0f);
                vertices.push_back(z);

                // UV
                vertices.push_back(u);
                vertices.push_back(v);
            }
        }


        // --------------------------------------------------------
        // Indices
        // --------------------------------------------------------

        std::vector<unsigned int> indices;

        indices.reserve(
            (meshWidth_ - 1) *
            (meshHeight_ - 1) *
            6
        );


        for (int row = 0;
            row < meshHeight_ - 1;
            ++row)
        {
            for (int col = 0;
                col < meshWidth_ - 1;
                ++col)
            {
                unsigned int topLeft =
                    row * meshWidth_ + col;

                unsigned int topRight =
                    topLeft + 1;

                unsigned int bottomLeft =
                    (row + 1) * meshWidth_ + col;

                unsigned int bottomRight =
                    bottomLeft + 1;


                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }


        indexCount_ =
            static_cast<int>(indices.size());


        // ========================================================
        // OPENGL BUFFERS
        // ========================================================

        glGenVertexArrays(
            1,
            &VAO_
        );

        glGenBuffers(
            1,
            &VBO_
        );

        glGenBuffers(
            1,
            &EBO_
        );


        glBindVertexArray(VAO_);


        // --------------------------------------------------------
        // Vertex buffer
        // --------------------------------------------------------

        glBindBuffer(
            GL_ARRAY_BUFFER,
            VBO_
        );

        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * sizeof(float),
            vertices.data(),
            GL_STATIC_DRAW
        );


        // --------------------------------------------------------
        // Position
        // --------------------------------------------------------

        glEnableVertexAttribArray(0);

        glVertexAttribPointer(
            0,
            3,
            GL_FLOAT,
            GL_FALSE,
            5 * sizeof(float),
            (void*)0
        );


        // --------------------------------------------------------
        // UV
        // --------------------------------------------------------

        glEnableVertexAttribArray(1);

        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            5 * sizeof(float),
            (void*)(3 * sizeof(float))
        );


        // --------------------------------------------------------
        // Index buffer
        // --------------------------------------------------------

        glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER,
            EBO_
        );

        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW
        );


        glBindVertexArray(0);
    }


    // ============================================================
    // DRAW
    // ============================================================

    void Terrain::draw(
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec3& lightPos,
        const glm::vec3& lightColor,
        const glm::mat4& model
    )
    {
        shader_.use();


        shader_.setMat4(
            "model",
            model
        );

        shader_.setMat4(
            "view",
            view
        );

        shader_.setMat4(
            "projection",
            projection
        );


        shader_.setVec3(
            "lightPos",
            lightPos
        );

        shader_.setVec3(
            "lightColor",
            lightColor
        );


        shader_.setVec2("uvMax", uvMax_);
        shader_.setVec2("uvMin", uvMin_);


        shader_.setFloat(
            "maxElevation",
            maxElevationMeters_
        );



        shader_.setVec2("meshTexelSize", glm::vec2(
            1.0f / static_cast<float>(meshWidth_ - 1),
            1.0f / static_cast<float>(meshHeight_ - 1)
        ));



        // --------------------------------------------------------
        // Heightmap texture
        // --------------------------------------------------------

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(
            GL_TEXTURE_2D,
            heightmapTexture_
        );

        shader_.setInt(
            "heightmap",
            0
        );

        shader_.setFloat("tileScale", 75.0f); // tune: bigger = each texture repeat covers more ground

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


        // --------------------------------------------------------
        // Draw
        // --------------------------------------------------------

        glBindVertexArray(VAO_);

        glDrawElements(
            GL_TRIANGLES,
            indexCount_,
            GL_UNSIGNED_INT,
            nullptr
        );

        glBindVertexArray(0);
    }


    GLuint loadTexture2D(const std::string& path) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        int w, h, channels;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 0);
        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D); // needed since tiled textures get viewed at many distances
            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load texture: " << path << "\n";
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // REPEAT, not CLAMP — this is what makes tiling work
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return tex;
    }

    void Terrain::loadTerrainTextures() {
        grassTex_ = loadTexture2D("textures/terrain/grass.png");
        rockTex_ = loadTexture2D("textures/terrain/rock.png");
        sandTex_ = loadTexture2D("textures/terrain/sand.png");
        snowTex_ = loadTexture2D("textures/terrain/snow.png");
    }
}