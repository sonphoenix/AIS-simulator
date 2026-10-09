#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <ixwebsocket/IXNetSystem.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <ais_sim/ais_client.hpp>
#include <ais_sim/ship_registry.hpp>
#include <ais_sim/dotenv.hpp>
#include <ais_sim/wave.hpp>
#include <ais_sim/terrain.hpp>
#include <ais_sim/flaremanager.hpp>
#include<ais_sim/cubemap.hpp>
#include<ais_sim/tile_fetcher.hpp>
#include <ais_sim/world_terrain.hpp>
#include <shaderClass.hpp>
#include <ais_sim/ocean.hpp>
#include <stb_image.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <ais_sim/model.hpp>

bool wireframe = false;
bool shiftPressedLast = false;
bool mapMode = false;
bool mapKeyLast = false, clickLast = false;
bool placeCameraInWorld = false;   // set when a world was just generated
float moveSpeed = 500.0f;          // camera speed in units (meters) per second

static glm::vec3 color_for(uint32_t mmsi) {
    float r = ((mmsi * 2654435761u) % 256) / 255.0f;
    float g = ((mmsi * 40503u) % 256) / 255.0f;
    float b = ((mmsi * 2246822519u) % 256) / 255.0f;
    return glm::vec3(r, g, b);
}

// Turns the mouse position into a world-space ray direction.
// (The ray starts at the camera position.)
glm::vec3 mouseRayDir(GLFWwindow* w, const glm::mat4& view, const glm::mat4& proj) {
    double mx, my; glfwGetCursorPos(w, &mx, &my);
    int ww, hh;    glfwGetWindowSize(w, &ww, &hh);
    float nx = 2.0f * (float)mx / ww - 1.0f;
    float ny = 1.0f - 2.0f * (float)my / hh;

    glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 a = inv * glm::vec4(nx, ny, -1.0f, 1.0f); a /= a.w;
    glm::vec4 b = inv * glm::vec4(nx, ny, 1.0f, 1.0f); b /= b.w;
    return glm::normalize(glm::vec3(b) - glm::vec3(a));
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

GLuint loadFlareTex(const char* path) {
    GLuint id;
    glGenTextures(1, &id);
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return id;
}


std::vector<unsigned int> buildOceanIndices(int N, int M) {
    int stride = N + 1;
    std::vector<unsigned int> indices;
    indices.reserve(N * M * 6);
    for (int row = 0; row < M; row++) {
        for (int col = 0; col < N; col++) {
            unsigned int i0 = row * stride + col;
            unsigned int i1 = row * stride + col + 1;
            unsigned int i2 = (row + 1) * stride + col;
            unsigned int i3 = (row + 1) * stride + col + 1;
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
            indices.push_back(i2);
        }
    }
    return indices;
}


// Camera variables
glm::vec3 cameraPos = glm::vec3(0.0f, 3000.0f, 3000.0f);
glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, -0.5f, -1.0f));
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f, pitch = -20.0f;
bool firstMouse = true;
float lastX = 400.0f, lastY = 300.0f;
float deltaTime = 0.0f, lastFrame = 0.0f;

// Map picking: mouse -> lat/lon on the world map plane (y = 0).
// Must live below the camera variables because it uses cameraPos.
// World map: x spans -5000..5000 for lon -180..180, -z is north (lat +90 at z = -2500).
bool pickLatLon(GLFWwindow* w, const glm::mat4& view, const glm::mat4& proj,
    double& lat, double& lon) {
    glm::vec3 dir = mouseRayDir(w, view, proj);
    if (dir.y > -0.0001f) return false;          // ray must point downward
    float t = -cameraPos.y / dir.y;
    glm::vec3 hit = cameraPos + dir * t;
    lon = hit.x / 10000.0 * 360.0;
    lat = -hit.z / 5000.0 * 180.0;
    return lat >= -90.0 && lat < 90.0 && lon >= -180.0 && lon < 180.0;
}



void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (mapMode) return;
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    xoffset *= 0.1f;
    yoffset *= 0.1f;
    yaw += xoffset;
    pitch += yoffset;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

void processInput(GLFWwindow* window) {
    float speed = moveSpeed * deltaTime;
    glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));

    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) cameraPos -= speed * right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += speed * right;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += speed * cameraUp;

    bool shiftPressedNow = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    if (shiftPressedNow && !shiftPressedLast) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }
    shiftPressedLast = shiftPressedNow;

    // Map mode toggle. GLFW_KEY_M is the physical US-layout "M" position
    // (the "," key on a French AZERTY keyboard).
    bool mapKeyNow = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mapKeyNow && !mapKeyLast) {
        mapMode = !mapMode;
        if (mapMode) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cameraPos = glm::vec3(0.0f, 9000.0f, 0.0f);
            cameraFront = glm::vec3(0.0f, -1.0f, 0.0f);
            cameraUp = glm::vec3(0.0f, 0.0f, -1.0f);   // north (-z) at the top of the screen
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

            if (placeCameraInWorld) {                  // a world was generated: go and look at it
                cameraPos = glm::vec3(0.0f, 8000.0f, 60000.0f);   // 8 km up, 60 km south of the center
                yaw = -90.0f;
                pitch = -8.0f;
                moveSpeed = 5000.0f;                   // 93 km is a long way at 500 per second
                placeCameraInWorld = false;
            }

            cameraFront = glm::normalize(glm::vec3(
                cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                sin(glm::radians(pitch)),
                sin(glm::radians(yaw)) * cos(glm::radians(pitch))));
            firstMouse = true;
        }
    }
    mapKeyLast = mapKeyNow;
}

int main() {
    ix::initNetSystem();

    std::string api_key = ais_sim::read_env_value(".env", "AISSTREAM_API_KEY");
    if (api_key.empty()) {
        std::cerr << "AISSTREAM_API_KEY not found in .env file.\n";
        ix::uninitNetSystem();
        return 1;
    }

    ais_sim::ShipRegistry registry;
    ais_sim::CoordinateConverter converter(37.5, -122.5);
    ais_sim::AisClient client("wss://stream.aisstream.io/v0/stream", api_key, 38.0, -123.0, 37.0, -122.0);
    client.on_message([&registry, &converter](const ais_sim::AisMessage& msg) {
        registry.update(msg);
        converter.convert(msg.lat, msg.lon);
        });
    client.connect();
    client.run();

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        ix::uninitNetSystem();
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 16);

    GLFWwindow* window = glfwCreateWindow(800, 600, "AIS Simulator", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        ix::uninitNetSystem();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        ix::uninitNetSystem();
        return 1;
    }
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // Shaders
    Shader shipShader("shaders/vertexshader.vs", "shaders/fragmentshader.fs");
    Shader skyboxShader("shaders/skyboxshader.vs", "shaders/skyboxshader.fs");
    Shader sunShader("shaders/sun.vs", "shaders/sun.fs");
    Shader oceanShader("shaders/ocean.vs", "shaders/ocean.fs");
    Shader terrainShader("shaders/terrain.vs", "shaders/terrain.fs");

    // Ship VAO/VBO
    GLuint shipVAO, shipVBO;
    glGenVertexArrays(1, &shipVAO);
    glGenBuffers(1, &shipVBO);
    glBindVertexArray(shipVAO);
    glBindBuffer(GL_ARRAY_BUFFER, shipVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Skybox setup
    float skyboxVertices[] = {
        -1, 1,-1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
        -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1,-1, -1, 1, 1, -1,-1, 1,
         1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1,  1,-1,-1,
        -1,-1, 1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1,-1, 1, -1,-1, 1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1,
        -1,-1,-1, -1,-1, 1,  1,-1,-1,  1,-1,-1, -1,-1, 1,  1,-1, 1
    };
    GLuint skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    std::vector<std::string> faces = {
        "textures/skybox/px.png", "textures/skybox/nx.png",
        "textures/skybox/py.png", "textures/skybox/ny.png",
        "textures/skybox/pz.png", "textures/skybox/nz.png"
    };
    stbi_set_flip_vertically_on_load(false);
    GLuint cubemapTexture = ais_sim::loadCubemap(faces);

    // Sun setup
    float sunVertices[] = {
        -0.5f, -0.5f,   0.5f, -0.5f,   0.5f, 0.5f,  -0.5f, 0.5f
    };
    unsigned int sunIndices[] = { 0, 1, 2,  2, 3, 0 };

    GLuint sunVAO, sunVBO, sunEBO;
    glGenVertexArrays(1, &sunVAO);
    glGenBuffers(1, &sunVBO);
    glGenBuffers(1, &sunEBO);
    glBindVertexArray(sunVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sunVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sunVertices), sunVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sunEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sunIndices), sunIndices, GL_STATIC_DRAW);

    //ocean setup
    ais_sim::Ocean ocean;



    glm::vec3 sundir = glm::normalize(glm::vec3(0.0f, 1.0f, -2.0f));
    glm::vec3 lampPos = sundir * 5000.0f;



    ais_sim::Terrain terrain(
        "textures/gebco_08_rev_elev_5400x2700.png",
        1024, 512,
        10000.0f, 5000.0f,
        300.0f,
        -180.0f, 180.0f,
        -90.0f, 90.0f
    );

    //model testing

    Shader modelShader("shaders/model.vs", "shaders/model.fs");
    ais_sim::Model shipModel("models/cargoship/Tug&Cargo Ship 01.obj");
    glm::mat4 modelLoc = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1000.0f, 0.0f));
    modelLoc = glm::scale(modelLoc, glm::vec3(50.0f));

    //flare setup
    std::vector<ais_sim::FlareSprite> flareSprites = {
        { loadFlareTex("textures/lensflare/tex1.png"), 0.04f, 0.7f },
        { loadFlareTex("textures/lensflare/tex2.png"), 0.30f, 0.35f },
        { loadFlareTex("textures/lensflare/tex3.png"), 0.06f, 0.6f },
        { loadFlareTex("textures/lensflare/tex4.png"), 0.15f, 0.5f },
        { loadFlareTex("textures/lensflare/tex5.png"), 0.03f, 0.9f },
        { loadFlareTex("textures/lensflare/tex6.png"), 0.35f, 0.3f },
        { loadFlareTex("textures/lensflare/tex7.png"), 0.10f, 0.5f },
        { loadFlareTex("textures/lensflare/tex8.png"), 0.20f, 0.4f },
        { loadFlareTex("textures/lensflare/tex9.png"), 0.04f, 0.8f },
    };

    ais_sim::FlareManager flaremanager(0.3f, flareSprites, "shaders/flare.vs", "shaders/flare.fs");

    // Map-mode tile highlight: one unit quad, scaled/moved by uniforms
    Shader highlightShader("shaders/highlight.vs", "shaders/highlight.fs");
    float quad[] = { 0,0,  1,0,  1,1,  0,1 };
    GLuint hlVAO, hlVBO;
    glGenVertexArrays(1, &hlVAO);
    glGenBuffers(1, &hlVBO);
    glBindVertexArray(hlVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hlVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);


    ais_sim::TileFetcher fetcher;
    std::unique_ptr<ais_sim::WorldTerrain> worldTerrain;   // empty until you click a tile

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        float aspect = fbHeight > 0 ? (float)fbWidth / (float)fbHeight : 1.0f;

        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        bool inWorld = (worldTerrain != nullptr) && !mapMode;   // looking at a generated world?

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        float nearPlane = mapMode ? std::max(1.0f, cameraPos.y * 0.05f)
            : (inWorld ? std::max(5.0f, cameraPos.y * 0.1f) : 0.1f);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, nearPlane, 100000.0f);

        // --- Map mode: hover + click on a tile ---
        bool hasHover = false;
        double hLat = 0.0, hLon = 0.0;
        if (mapMode) hasHover = pickLatLon(window, view, projection, hLat, hLon);

        bool clickNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (hasHover && clickNow && !clickLast) {
            std::cout << "\nclicked at lat " << hLat << ", lon " << hLon << "\n";

            if (std::abs(hLat) > 84.0) {
                std::cout << "too close to the poles for these tiles\n";
            }
            else {
                ais_sim::TileCoord t = ais_sim::TileFetcher::latLonToTile(hLat, hLon, 10);

                double lat0, lon0, lat1, lon1;
                ais_sim::TileFetcher::tileToLatLon(t.x, t.y, 10, lat0, lon0);           // top-left corner
                ais_sim::TileFetcher::tileToLatLon(t.x + 1, t.y + 1, 10, lat1, lon1);   // bottom-right corner
                std::cout << "elevation tile " << t.x << ", " << t.y << " covers lat "
                    << lat1 << " to " << lat0 << ", lon " << lon0 << " to " << lon1 << "\n";

                // The window freezes here while the 9 tiles download (only the first time).
                ais_sim::HeightGrid grid = fetcher.fetchBlock3x3(10, t);
                if (!grid.meters.empty()) {
                    float mn = *std::min_element(grid.meters.begin(), grid.meters.end());
                    float mx = *std::max_element(grid.meters.begin(), grid.meters.end());
                    std::cout << "block " << grid.width << "x" << grid.height
                        << ", min " << mn << " m, max " << mx << " m\n";
                    std::cout << "centre pixel: " << grid.at(384, 384) << " m\n";
                    std::cout << "across the left seam:  " << grid.at(255, 384) << " | " << grid.at(256, 384) << "\n";
                    std::cout << "across the top seam:   " << grid.at(384, 255) << " | " << grid.at(384, 256) << "\n";

                    // Build the terrain of the generated world from the block
                    worldTerrain = std::make_unique<ais_sim::WorldTerrain>(grid, (lat0 + lat1) * 0.5, 10);
                    placeCameraInWorld = true;
                    std::cout << "world ready: press the map key to go and look at it\n";
                }
                else {
                    std::cerr << "could not build the block of tiles\n";
                }
            }
        }
        clickLast = clickNow;

        // Update Ship Dynamic Buffer
        std::vector<ais_sim::ShipState> ships = registry.snapshot();
        std::vector<float> shipVertices;
        shipVertices.reserve(ships.size() * 6);
        for (const auto& ship : ships) {
            shipVertices.push_back((float)ship.posX);
            shipVertices.push_back(0.0f);
            shipVertices.push_back((float)ship.posZ);
            glm::vec3 col = color_for(ship.mmsi);
            shipVertices.push_back(col.r);
            shipVertices.push_back(col.g);
            shipVertices.push_back(col.b);
        }
        glBindBuffer(GL_ARRAY_BUFFER, shipVBO);
        glBufferData(GL_ARRAY_BUFFER, shipVertices.size() * sizeof(float), shipVertices.data(), GL_DYNAMIC_DRAW);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Skybox
        skyboxShader.use();
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skyboxShader.setMat4("view", glm::mat4(glm::mat3(view)));
        skyboxShader.setMat4("projection", projection);
        skyboxShader.setInt("skybox", 0);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // 2. Ocean: covers the map in map mode, follows the camera in the old scene,
        //    and is hidden inside a generated world for now (that water comes later)
        if (mapMode) {
            ocean.update((float)glfwGetTime());
            ocean.drawMap(view, projection, cameraPos, lampPos, 10000.0f, 5000.0f);
        }
        else if (!inWorld) {
            ocean.update((float)glfwGetTime());
            ocean.draw(view, projection, cameraPos, lampPos);
        }
        else {
            ocean.update((float)glfwGetTime());   // the FFT was never running in the world before
            glm::vec3 worldLight = sundir * 1.0e7f;
            // flat backdrop out to the horizon, 4 m under the waves to avoid z-fighting
            ocean.drawPlane(view, projection, cameraPos, worldLight,
                0.0f, 0.0f, worldTerrain->halfSizeMeters() * 2.0f, -30.0f);
            // real waves around the camera (radius 6 tiles = 13 km across)
            ocean.drawWorld(view, projection, cameraPos, worldLight, 0.0f, 6,
                worldTerrain->heightTexture(), worldTerrain->metersPerTexel());
        }


        // Ships, sun and lens flare belong to the old scene only
        if (!mapMode && !inWorld) {
            // 2. Ships
            shipShader.use();
            shipShader.setMat4("model", glm::mat4(1.0f));
            shipShader.setMat4("view", view);
            shipShader.setMat4("projection", projection);
            glBindVertexArray(shipVAO);
            glDrawArrays(GL_POINTS, 0, (GLsizei)ships.size());

            // 4. Sun Billboard
            glm::vec3 camRight = glm::normalize(glm::cross(cameraFront, cameraUp));
            glm::vec3 camUpTrue = glm::cross(camRight, cameraFront);

            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);

            sunShader.use();
            sunShader.setVec3("sunPos", lampPos);
            sunShader.setVec3("camRight", camRight);
            sunShader.setVec3("camUp", camUpTrue);
            sunShader.setFloat("size", 3000.0f);
            sunShader.setVec3("sunColor", glm::vec3(1.0f, 1.0, 1.0f));
            sunShader.setMat4("view", view);
            sunShader.setMat4("projection", projection);

            glBindVertexArray(sunVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            // lens flare
            flaremanager.render(view, projection, lampPos, aspect);
        }

        // 5. Terrain: the generated world if we are looking at one, otherwise the world map
        if (inWorld) {
            worldTerrain->draw(view, projection, sundir, glm::vec3(1.0f, 0.85f, 0.5f));
        }
        else {
            glm::mat4 terrainModel = glm::mat4(1.0f);
            terrain.draw(view, projection, lampPos, glm::vec3(1.0f, 0.85f, 0.5f), terrainModel);
        }

        // Map-mode highlight of the hovered tile
        if (mapMode && hasHover) {
            const double tileDeg = 5.0;   // must divide 90 evenly: 1, 2, 3, 5, 6, 9, 10, 15, 30...
            float tLat = (float)(std::floor(hLat / tileDeg) * tileDeg);
            float tLon = (float)(std::floor(hLon / tileDeg) * tileDeg);
            float x0 = tLon / 360.0f * 10000.0f;
            float x1 = (tLon + (float)tileDeg) / 360.0f * 10000.0f;
            float z0 = -(tLat + (float)tileDeg) / 180.0f * 5000.0f;   // north edge
            float z1 = -tLat / 180.0f * 5000.0f;                      // south edge

            glDisable(GL_DEPTH_TEST);                     // the terrain would hide it otherwise
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            highlightShader.use();
            highlightShader.setMat4("view", view);
            highlightShader.setMat4("projection", projection);
            highlightShader.setVec2("origin", glm::vec2(x0, z0));
            highlightShader.setVec2("size", glm::vec2(x1 - x0, z1 - z0));
            highlightShader.setFloat("height", 0.0f);

            glBindVertexArray(hlVAO);
            highlightShader.setVec3("color", glm::vec3(1.0f, 0.9f, 0.2f));
            highlightShader.setFloat("alpha", 0.1f);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);          // yellow fill
            highlightShader.setVec3("color", glm::vec3(1.0f));
            highlightShader.setFloat("alpha", 1.0f);
            glDrawArrays(GL_LINE_LOOP, 0, 4);             // white outline

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        // 6. model test (old scene only)
        if (!mapMode && !inWorld) {
            modelShader.use();
            modelShader.setMat4("model", modelLoc);
            modelShader.setMat4("view", view);
            modelShader.setMat4("projection", projection);
            modelShader.setVec3("sunPos", lampPos);
            shipModel.draw();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup: free GPU objects while the OpenGL context still exists
    worldTerrain.reset();
    glDeleteVertexArrays(1, &hlVAO);
    glDeleteBuffers(1, &hlVBO);
    glDeleteVertexArrays(1, &shipVAO);
    glDeleteBuffers(1, &shipVBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &sunVAO);
    glDeleteBuffers(1, &sunVBO);
    glDeleteBuffers(1, &sunEBO);
    ocean.release();
    glDeleteTextures(1, &cubemapTexture);

    glfwDestroyWindow(window);
    glfwTerminate();
    ix::uninitNetSystem();
    return 0;
}