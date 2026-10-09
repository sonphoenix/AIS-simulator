#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <shaderClass.hpp>
#include <memory>
#include <vector>

namespace ais_sim {

    struct FlareSprite {
        GLuint texture;
        float scale;      // size of this sprite, in NDC units (before aspect correction)
        float alphaScale; // per-sprite brightness multiplier (0..1), on top of the global fade
    };

    class FlareManager {
    public:
        // spacing: how far apart each successive flare sits along the sun->center line.
        // sprites: index 0 sits closest to the sun's own position (small spacing multiple),
        //          later indices march through screen center and out the other side.
        FlareManager(float spacing, std::vector<FlareSprite> sprites,
            const char* vertPath, const char* fragPath);
        ~FlareManager();

        FlareManager(const FlareManager&) = delete;
        FlareManager& operator=(const FlareManager&) = delete;


        void render(const glm::mat4& view, const glm::mat4& projection,
            const glm::vec3& sunWorldPos, float aspect);

    private:
        float spacing;
        std::vector<FlareSprite> sprites;
        std::unique_ptr<Shader> shader;
        GLuint quadVAO = 0, quadVBO = 0;

        void setupQuad();
    };

} // namespace ais_sim