#version 460 core
layout(location = 0) in vec2 aPos;

uniform vec2 center;
uniform vec2 scale;

out vec2 TexCoords;

void main() {
    TexCoords = aPos * 0.5 + 0.5;
    vec2 pos = center + aPos * scale;
    gl_Position = vec4(pos, 0.0, 1.0);
}
