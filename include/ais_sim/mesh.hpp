#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace ais_sim {

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, GLuint diffuseTexture);
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&& other) noexcept;
        Mesh& operator=(Mesh&& other) noexcept;

        void draw() const;

    private:
        GLuint VAO = 0, VBO = 0, EBO = 0;
        GLuint diffuseTexture = 0;
        size_t indexCount = 0;

        void setupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    };

} // namespace ais_sim