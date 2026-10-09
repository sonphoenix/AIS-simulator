#version 460 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D flareTex;
uniform float alpha;

void main() {
    vec4 texColor = texture(flareTex, TexCoords);
    vec3 finalColor = texColor.rgb * texColor.a * alpha;
    FragColor = vec4(finalColor, 1.0);
}