#version 450

layout(location = 0) out vec2 uv;


const vec2 positions[4] = vec2[](
    vec2(-1.0f,  1.0f)  // Top-left
    vec2(-1.0f, -1.0f), // Bottom-left
    vec2( 1.0f, -1.0f), // Bottom-right
    vec2( 1.0f,  1.0f), // Top-right
);

const vec2 uvs[4] = vec2[](
    vec2(0.0f, 1.0f)  // Top-left
    vec2(0.0f, 0.0f), // Bottom-left
    vec2(1.0f, 0.0f), // Bottom-right
    vec2(1.0f, 1.0f), // Top-right
);

const uint indices[6] = uint[](
    0, 1, 2,
    0, 2, 3
);


void main() {
    uint index = indices[gl_VertexIndex];
    gl_Position = vec4(positions[index], 0.0f, 1.0f);
    uv = uvs[index];
}
