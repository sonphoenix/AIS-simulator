#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

namespace ais_sim {

    // Loads 6 face images into one GL_TEXTURE_CUBE_MAP. faces must be ordered:
    // +X, -X, +Y, -Y, +Z, -Z (matches GL_TEXTURE_CUBE_MAP_POSITIVE_X and onward).
    // Returns the texture ID; caller owns it and is responsible for
    // glDeleteTextures — this is a plain loader, not an owning resource class,
    GLuint loadCubemap(const std::vector<std::string>& faces);

} // namespace ais_sim