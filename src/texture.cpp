#include <ais_sim/texture.hpp>
#include <stb_image.h>
#include <iostream>

namespace ais_sim {

    Texture::Texture(const std::string& path, bool generateMipmaps, GLenum texType)
        : texType(texType)
    {
        glGenTextures(1, &texture_ID);

        int width, height, nchannels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nchannels, 0);

        if (data) {
            GLenum format = (nchannels == 4) ? GL_RGBA : (nchannels == 3 ? GL_RGB : GL_RED);
            glBindTexture(texType, texture_ID);
            glTexImage2D(texType, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (generateMipmaps)
                glGenerateMipmap(texType);

            glTexParameteri(texType, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(texType, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(texType, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(texType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            stbi_image_free(data);
        }
        else {
            std::cerr << "Texture failed to load: " << path << "\n";
        }
    }

    Texture::~Texture() {
        if (texture_ID) glDeleteTextures(1, &texture_ID);
    }

    Texture::Texture(Texture&& other) noexcept
        : texture_ID(other.texture_ID), texType(other.texType)
    {
        other.texture_ID = 0;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (texture_ID) glDeleteTextures(1, &texture_ID);
            texture_ID = other.texture_ID;
            texType = other.texType;
            other.texture_ID = 0;
        }
        return *this;
    }

    void Texture::activate(GLenum activeTex) const {
        glActiveTexture(activeTex);
        glBindTexture(texType, texture_ID);
    }

} // namespace ais_sim