#pragma once

#include <glad/glad.h> 

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include<glm/glm.hpp>

class Shader
{
public:
    // the program ID
    unsigned int ID;

    //uniform cache
    mutable std::unordered_map < std::string , GLint> uniformCache;

    // constructor reads and builds the shader
    Shader(const char* vertexPath, const char* fragmentPath);
    // use/activate the shader
    void use();

    GLint getUniformLocation(const std::string& name) const;
    // utility uniform functions
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& vec2) const;
    void setVec3(const std::string& name, const glm::vec3& vec) const;

    void setMat4(const std::string& name, const glm::mat4& mat) const;
};

