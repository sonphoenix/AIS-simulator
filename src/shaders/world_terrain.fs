#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TileUV;
in float HeightM;

uniform sampler2D grassTex;
uniform sampler2D rockTex;
uniform sampler2D sandTex;
uniform sampler2D snowTex;

uniform vec3 sunDir;       // direction TOWARD the sun, any length
uniform vec3 lightColor;

out vec4 FragColor;

void main()
{
    vec3 norm = normalize(Normal);
    float slope = 1.0 - clamp(norm.y, 0.0, 1.0);

    vec3 grass = texture(grassTex, TileUV).rgb;
    vec3 sand  = texture(sandTex,  TileUV).rgb;
    vec3 rock  = texture(rockTex,  TileUV).rgb;
    vec3 snow  = texture(snowTex,  TileUV).rgb;

    // Colour by real height in meters. Tune these numbers by eye.
    vec3 c = mix(sand, grass, smoothstep(0.0, 8.0, HeightM));        // beach to grass
    c = mix(c, rock, smoothstep(1200.0, 2500.0, HeightM));           // high ground turns rocky
    c = mix(c, rock, smoothstep(0.35, 0.75, slope));                 // steep slopes are rock
    c = mix(c, snow, smoothstep(2500.0, 3500.0, HeightM));           // snow line

    // Below sea level: darker and bluer with depth (only visible once the water is see-through)
    float depth = clamp(-HeightM / 200.0, 0.0, 1.0);
    c = mix(c, c * vec3(0.15, 0.30, 0.45), depth);

    // The sun is far away, so treat it as a direction, not a point
    float diffuse = max(dot(norm, normalize(sunDir)), 0.0);
    float lighting = 0.30 + 0.70 * diffuse;

    FragColor = vec4(c * lighting * lightColor, 1.0);
}