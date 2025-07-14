#version 450

layout(location = 0) out vec2 uv;


void main() {
    uv = vec2(
        float((gl_VertexIndex << 1u) & 2u),
        float(gl_VertexIndex & 2u)
    );
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}
