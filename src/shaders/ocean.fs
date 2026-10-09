#version 460 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float heightMin;
uniform float heightMax;

// Terrain mask (generated world only)
uniform int useMask;
uniform sampler2D terrainHeight;
uniform float terrainMPT;
uniform float seaLevel;

// Terrain height in meters at a world xz position (bilinear, same layout as WorldTerrain's mesh).
float terrainAt(vec2 xz) {
    ivec2 size = textureSize(terrainHeight, 0);
    vec2 p = xz / terrainMPT + vec2(size / 2);
    if (p.x < 0.0 || p.y < 0.0 || p.x > float(size.x - 1) || p.y > float(size.y - 1))
        return -10000.0;                                  // outside the block: no land
    ivec2 i = clamp(ivec2(floor(p)), ivec2(0), size - 2);
    vec2 f = p - vec2(i);
    float h00 = texelFetch(terrainHeight, i, 0).r;
    float h10 = texelFetch(terrainHeight, i + ivec2(1, 0), 0).r;
    float h01 = texelFetch(terrainHeight, i + ivec2(0, 1), 0).r;
    float h11 = texelFetch(terrainHeight, i + ivec2(1, 1), 0).r;
    return mix(mix(h00, h10, f.x), mix(h01, h11, f.x), f.y);
}

void main() {
    if (useMask == 1 && terrainAt(FragPos.xz) > seaLevel) discard;

    vec3 norm = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    if (dot(norm, viewDir) < 0.0) norm = -norm;

    vec3 skyColor = vec3(0.65, 0.80, 0.95);
    vec3 shallowColor = vec3(0.05, 0.15, 0.30);
    vec3 deepColor = vec3(0.01, 0.02, 0.06);
    float relHeight = clamp((FragPos.y - heightMin) / (heightMax - heightMin), 0.0, 1.0);
    vec3 heightColor = mix(deepColor, shallowColor, relHeight);

    float refCoeff = pow(max(dot(norm, viewDir), 0.0), 0.3);
    vec3 reflectColor = (1.0 - refCoeff) * skyColor;

    vec3 refDir = reflect(-L, norm);
    float specCoeff = clamp(pow(max(dot(viewDir, refDir), 0.0), 128.0) * 3.0, 0.0, 1.0);
    vec3 specular = lightColor * specCoeff;

    vec3 combined = heightColor + reflectColor;
    combined = combined * (1.0 - specCoeff) + specular;

    FragColor = vec4(combined, 1.0);
}