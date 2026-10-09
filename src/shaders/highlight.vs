#version 460 core
layout(location = 0) in vec2 aPos;      // corner of a unit square (0..1)
uniform vec2 origin;                     // x, z of the tile's corner
uniform vec2 size;                       // tile width (x) and depth (z)
uniform float height;
uniform mat4 view;
uniform mat4 projection;
void main() {
    vec3 p = vec3(origin.x + aPos.x * size.x, height, origin.y + aPos.y * size.y);
    gl_Position = projection * view * vec4(p, 1.0);
}