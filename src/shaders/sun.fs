#version 460 core
in vec2 localPos;
out vec4 FragColor;

uniform vec3 sunColor;

void main() {
    float d = length(localPos) * 2.0;
    float glow = pow(clamp(1.0 - d, 0.0, 1.0), 3.0);
    FragColor = vec4(sunColor * glow, 1.0);
}