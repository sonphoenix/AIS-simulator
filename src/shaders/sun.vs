#version 460 core
layout (location = 0) in vec2 aPos;

uniform vec3 sunPos;
uniform vec3 camRight;
uniform vec3 camUp;
uniform float size;
uniform mat4 view;
uniform mat4 projection;

out vec2 localPos;

void main() {
    localPos = aPos;
    vec3 worldPos = sunPos + (aPos.x * camRight + aPos.y * camUp) * size;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}