#version 460 core

layout(location = 0) in vec3 aPos;        // x and z in meters, y unused
layout(location = 1) in vec2 aTexCoord;   // 0..1 across the whole grid

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D heightmap;      // one float channel: height in meters
uniform float metersPerTexel;     // ground distance between two neighbouring samples
uniform float heightScale;        // 1.0 = real heights, 2.0 = exaggerated
uniform float tileScale;          // meters covered by one repeat of the ground textures

out vec3 FragPos;
out vec3 Normal;
out vec2 TileUV;
out float HeightM;                // height in meters, passed on to the fragment shader

float heightAt(ivec2 t) {
    ivec2 size = textureSize(heightmap, 0);
    t = clamp(t, ivec2(0), size - 1);
    return texelFetch(heightmap, t, 0).r;     // exact sample, no filtering
}

void main()
{
    ivec2 size = textureSize(heightmap, 0);
    ivec2 t = ivec2(round(aTexCoord * vec2(size - 1)));   // this vertex's own texel

    float h  = heightAt(t);
    float hL = heightAt(t + ivec2(-1, 0));
    float hR = heightAt(t + ivec2( 1, 0));
    float hD = heightAt(t + ivec2(0, -1));
    float hU = heightAt(t + ivec2(0,  1));

    // slope = rise over run, both in meters (the run is 2 texels wide)
    float sx = (hR - hL) * heightScale / (2.0 * metersPerTexel);
    float sz = (hU - hD) * heightScale / (2.0 * metersPerTexel);
    vec3 normal = normalize(vec3(-sx, 1.0, -sz));

    HeightM = h;
    vec3 worldPos = vec3(aPos.x, h * heightScale, aPos.z);
    FragPos = vec3(model * vec4(worldPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    TileUV = aPos.xz / tileScale;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}