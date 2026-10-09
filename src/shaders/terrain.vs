#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform sampler2D heightmap;
uniform float maxElevation;
uniform vec2 heightmapTexelSize;
uniform vec2 meshTexelSize;

uniform vec2 uvMin;
uniform vec2 uvMax;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec2 TileUV; 

uniform float tileScale; 


float getHeight(vec2 uv) {
    // Average a small neighborhood first, to remove quantization noise
    // before the pow curve (which is very steep near 0 and would
    // otherwise amplify that noise into visible spikes).
    float h = 0.0;
    h += texture(heightmap, uv).r;
    h += texture(heightmap, uv + vec2(meshTexelSize.x, 0.0)).r;
    h += texture(heightmap, uv - vec2(meshTexelSize.x, 0.0)).r;
    h += texture(heightmap, uv + vec2(0.0, meshTexelSize.y)).r;
    h += texture(heightmap, uv - vec2(0.0, meshTexelSize.y)).r;
    h /= 5.0;

    float boosted = pow(h, 0.6); // gentler than 0.4, less amplification
    return boosted * maxElevation;
}
void main()
{   
    vec2 sampleUV = mix(uvMin, uvMax, aTexCoord);
    float height = getHeight(sampleUV);

    vec2 texelUV = (uvMax - uvMin) * meshTexelSize;
    float hL = getHeight(sampleUV - vec2(texelUV.x, 0.0));
    float hR = getHeight(sampleUV + vec2(texelUV.x, 0.0));
    float hD = getHeight(sampleUV - vec2(0.0, texelUV.y));
    float hU = getHeight(sampleUV + vec2(0.0, texelUV.y));
    float dx = hR - hL;
    float dz = hU - hD;

    vec3 normal = normalize(vec3(-dx, 2.0, -dz));

    vec3 worldPos = vec3(aPos.x, height, aPos.z);
    FragPos = vec3(model * vec4(worldPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;

    TexCoord = sampleUV; // pass the REMAPPED coordinate, not raw aTexCoord

    gl_Position = projection * view * vec4(FragPos, 1.0);
    TileUV = aPos.xz / tileScale;
}