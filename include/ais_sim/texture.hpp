#pragma once
#include <glad/glad.h>
#include <string>

namespace ais_sim {

    class Texture {
    public:
        Texture(const std::string& path, bool generateMipmaps, GLenum texType);
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;

        // texType is remembered from construction — no need to pass it again
        void activate(GLenum activeTex) const;

        GLuint getTextureId() const { return texture_ID; }

    private:
        GLuint texture_ID = 0;
        GLenum texType;
    };

} // namespace ais_sim