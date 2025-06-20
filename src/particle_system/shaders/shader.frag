#version 450

layout(location = 0) in vec3 in_color;

layout(location = 0) out vec4 frag_color;


void main() {
    vec2 coord = gl_PointCoord - vec2(0.5f, 0.5f);
    frag_color = vec4(in_color, 0.5f - length(coord));
}
