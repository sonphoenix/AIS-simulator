#include <ais_sim/mesh.hpp>
#include<iostream>
namespace ais_sim {

    Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, GLuint diffuseTexture)
        : diffuseTexture(diffuseTexture)
    {
        indexCount = indices.size();
        setupMesh(vertices, indices);
    }

    Mesh::~Mesh() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        // diffuseTexture intentionally NOT deleted here — Model owns it
    }

    Mesh::Mesh(Mesh&& other) noexcept
        : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO),
        diffuseTexture(other.diffuseTexture), indexCount(other.indexCount)
    {
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.diffuseTexture = 0;
    }

    Mesh& Mesh::operator=(Mesh&& other) noexcept {
        if (this != &other) {
            if (VAO) glDeleteVertexArrays(1, &VAO);
            if (VBO) glDeleteBuffers(1, &VBO);
            if (EBO) glDeleteBuffers(1, &EBO);

            VAO = other.VAO;
            VBO = other.VBO;
            EBO = other.EBO;
            diffuseTexture = other.diffuseTexture;
            indexCount = other.indexCount;

            other.VAO = 0;
            other.VBO = 0;
            other.EBO = 0;
            other.diffuseTexture = 0;
        }
        return *this;
    }

    void Mesh::setupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

        glBindVertexArray(0);
    }

    void Mesh::draw() const {
        //std::cout << " \n drawing time saar \n";
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseTexture);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

} // namespace ais_sim