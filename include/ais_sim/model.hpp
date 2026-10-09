#pragma once
#include <ais_sim/mesh.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <vector>
#include <unordered_map>
#include<ais_sim/texture.hpp>

namespace ais_sim {

    class Model {
    public:
        explicit Model(const std::string& path);
        //~Model();

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;
        Model(Model&&) = default;
        Model& operator=(Model&&) = default;

        void draw() const;

    private:
        std::vector<Mesh> meshes;
        std::string directory;
        std::unordered_map<std::string, Texture> loadedTextures; // path -> GL texture id, avoids reloading

        void loadModel(const std::string& path);
        void processNode(aiNode* node, const aiScene* scene);
        Mesh processMesh(aiMesh* mesh, const aiScene* scene);
        GLuint loadMaterialTexture(aiMaterial* mat, aiTextureType type);
        GLuint loadTextureFromFile(const std::string& filename);
    };

} // namespace ais_sim