#version 460 core
in vec2 TexCoords;
in vec3 Normal;
in vec3 FragPos;
uniform vec3 sunPos;
out vec4 FragColor;


uniform sampler2D diffuseTexture;

void main() {
    vec3 lightDir = normalize(sunPos-FragPos);
    float diff = max(dot(normalize(Normal), lightDir), 0.2);
    vec4 texColor = texture(diffuseTexture, TexCoords);
    FragColor = vec4(texColor.rgb * diff, texColor.a);
}