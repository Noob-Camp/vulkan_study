#version 450

layout(location = 0) out vec2 outUV;


void main() {
    const vec2 positions[4] = vec2[](
        vec2(-1.0f, -1.0f), // Bottom-left
        vec2( 1.0f, -1.0f), // Bottom-right
        vec2( 1.0f,  1.0f), // Top-right
        vec2(-1.0f,  1.0f)  // Top-left
    );

    const vec2 uvs[4] = vec2[](
        vec2(0.0f, 0.0f), // Bottom-left
        vec2(1.0f, 0.0f), // Bottom-right
        vec2(1.0f, 1.0f), // Top-right
        vec2(0.0f, 1.0f)  // Top-left
    );
    const int indices[6] = int[](0, 1, 2, 0, 2, 3);

    gl_Position = vec4(positions[indices[gl_VertexIndex]], 0.0f, 1.0f);
    outUV = uvs[indices[gl_VertexIndex]];
}
