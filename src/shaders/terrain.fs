#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec2 TileUV;

uniform sampler2D grassTex;
uniform sampler2D rockTex;
uniform sampler2D sandTex;
uniform sampler2D snowTex;

out vec4 FragColor;

uniform sampler2D heightmap;

uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float maxElevation;

void main()
{
    vec3 norm = normalize(Normal);
    float rawHeight = texture(heightmap, TexCoord).r;
float height = pow(rawHeight, 0.6); // match the vertex shader's boost exactly
    float upness = clamp(dot(norm, vec3(0.0, 1.0, 0.0)), 0.0, 1.0);
    float slope = 1.0 - upness;

    vec3 grass = texture(grassTex, TileUV).rgb;
    vec3 dry   = texture(sandTex, TileUV).rgb;
    vec3 rock  = texture(rockTex, TileUV).rgb;
    vec3 snow  = texture(snowTex, TileUV).rgb;

    vec3 terrainColor;
    if (height < 0.30) {
        float t = smoothstep(0.05, 0.30, height);
        terrainColor = mix(grass, dry, t);
    } else {
        float t = smoothstep(0.30, 0.65, height);
        terrainColor = mix(dry, rock, t);
    }

    float rockFactor = smoothstep(0.35, 0.75, slope);
    terrainColor = mix(terrainColor, rock, rockFactor);

float snowHeight = smoothstep(0.55, 0.98, height); // now checks HIGH elevation
float snowFactor = snowHeight; // drop the upness requirement entirely
terrainColor = mix(terrainColor, snow, snowFactor);
    terrainColor = mix(terrainColor, snow, snowFactor);


    // --------------------------------------------------------
    // Lighting
    // --------------------------------------------------------

    vec3 lightDir = normalize(
        lightPos - FragPos
    );

    float diffuse = max(
        dot(norm, lightDir),
        0.0
    );

    float lighting =
        0.30 +
        diffuse * 0.70;


    vec3 finalColor =
        terrainColor *
        lighting *
        lightColor;

    FragColor = vec4(
        finalColor,
        1.0
    );
}