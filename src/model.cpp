#include <ais_sim/model.hpp>
#include <stb_image.h>
#include <iostream>
#include <filesystem>

namespace ais_sim {

    Model::Model(const std::string& path) {
        loadModel(path);
    }


    void Model::loadModel(const std::string& path) {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenNormals |          // from the tutorial's list
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << "\n";
            return;
        }

        directory = std::filesystem::path(path).parent_path().string();
        processNode(scene->mRootNode, scene);


    }

    void Model::processNode(aiNode* node, const aiScene* scene) {

        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
           // std::cout << "processing node i= " << i << "\n";
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
       // std::cout << "\n children processing time \n";
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
           // std::cout << "processing CHILD node i= " << i << "\n";
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
        stbi_set_flip_vertically_on_load(true);
       // std::cout << "aiMesh reports " << mesh->mNumVertices << " vertices, " << mesh->mNumFaces << " faces\n";
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex v;
            v.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->HasNormals())
                v.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            else
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            if (mesh->mTextureCoords[0])
                v.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            else
                v.texCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        //std::cout << "loading model with :" << vertices.size() << " vertices and :" << indices.size() << " indices \n";


        GLuint diffuseTex = 0;
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            diffuseTex = loadMaterialTexture(material, aiTextureType_DIFFUSE);
        }

        return Mesh(vertices, indices, diffuseTex);
    }

    GLuint Model::loadMaterialTexture(aiMaterial* mat, aiTextureType type) {
        if (mat->GetTextureCount(type) == 0)
            return 0;

        aiString str;
        mat->GetTexture(type, 0, &str);
        std::string filename = directory + "/" + std::string(str.C_Str());

        return loadTextureFromFile(filename);
    }

    GLuint Model::loadTextureFromFile(const std::string& filename) {
       // std::cout << "time to load texture";
        auto it = loadedTextures.find(filename);
        if (it != loadedTextures.end()) {
            return it->second.getTextureId();
        }
       // std::cout << "texture file :" << filename << "\n";
        auto [insertedIt, success] = loadedTextures.try_emplace(filename, filename, true, GL_TEXTURE_2D);
       // std::cout << "textures added size :"<< loadedTextures.size()<<"\n";
        return insertedIt->second.getTextureId();
    }

    void Model::draw() const {
        for (const auto& mesh : meshes) {
            mesh.draw();
        }
    }

} // namespace ais_sim