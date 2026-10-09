#include <ais_sim/flaremanager.hpp>

namespace ais_sim {

    FlareManager::FlareManager(float spacing, std::vector<FlareSprite> sprites,
        const char* vertPath, const char* fragPath)
        : spacing(spacing), sprites(std::move(sprites))
    {
        shader = std::make_unique<Shader>(vertPath, fragPath);
        setupQuad();
    }

    FlareManager::~FlareManager() {
        if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
        if (quadVBO) glDeleteBuffers(1, &quadVBO);
    }

    void FlareManager::setupQuad() {

        float quadVerts[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f,
            -1.0f, -1.0f
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);

        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

        glBindVertexArray(0);
    }

    // Ports FlareManager.convertToScreenSpace() from the Java version: project world
    // space through view/projection, then do the perspective divide by hand to get
    // NDC (-1..1) coordinates. Returns false (matching the Java "return null") when
    // the point is behind the camera, since dividing by a negative/zero w there
    // would give a garbage screen position.
    static bool worldToNDC(const glm::vec3& worldPos, const glm::mat4& view,
        const glm::mat4& projection, glm::vec2& outNDC)
    {
        glm::vec4 clip = projection * view * glm::vec4(worldPos, 1.0f);
        if (clip.w <= 0.0f) return false;
        outNDC = glm::vec2(clip.x / clip.w, clip.y / clip.w);
        return true;
    }

    void FlareManager::render(const glm::mat4& view, const glm::mat4& projection,
        const glm::vec3& sunWorldPos, float aspect)
    {
        glm::vec2 sunNDC;
        if (!worldToNDC(sunWorldPos, view, projection, sunNDC))
            return; // sun behind camera 

        const glm::vec2 centerScreen(0.0f, 0.0f);
        glm::vec2 sunToCenter = centerScreen - sunNDC;


        float brightness = 1.0f - (glm::length(sunToCenter) / 1.4f);
        if (brightness <= 0.0f)
            return;

        glDisable(GL_DEPTH_TEST); // sprites drawn at a fake NDC z; real depth testing
        // against scene geometry isn't meaningful here
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); // additive — overlapping glows brighten, don't occlude

        shader->use();
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        shader->setInt("flareTex", 0);

        for (size_t i = 0; i < sprites.size(); i++) {
            glm::vec2 direction = sunToCenter * (static_cast<float>(i) * spacing);
            glm::vec2 flarePos = sunNDC + direction;
            //std::cout << "flare num : " << i << " posx:" << flarePos.x << " posY:" << flarePos.y << "\n";

            // aspect correction so sprites stay circular instead of stretching
            // to match a non-square viewport
            glm::vec2 scale(sprites[i].scale, sprites[i].scale * aspect);

            shader->setVec2("center", flarePos);
            shader->setVec2("scale", scale);
            shader->setFloat("alpha", brightness * sprites[i].alphaScale);

            glBindTexture(GL_TEXTURE_2D, sprites[i].texture);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

} // namespace ais_sim