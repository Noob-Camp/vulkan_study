// #version 450

// layout(location = 0) in vec2 in_position;
// layout(location = 1) in vec4 in_color;

// layout(location = 0) out vec3 out_color;


// void main() {
//     gl_PointSize = 14.0f;
//     gl_Position = vec4(in_position.xy, 1.0f, 1.0f);
//     out_color = in_color.rgb;
// }




#version 450

// We don't need any explicit vertex inputs for this full-screen quad trick.
// The vertices are generated internally using gl_VertexIndex.

layout(location = 0) out vec2 outUV; // Output texture coordinates to the fragment shader

void main() {
    const vec2 positions[4] = vec2[](
        vec2(-1.0f, -1.0f), // Bottom-left
        vec2( 1.0f, -1.0f), // Bottom-right
        vec2( 1.0f,  1.0f), // Top-right
        vec2(-1.0f,  1.0f)  // Top-left
    );

    // Define corresponding UV coordinates (0 to 1)
    const vec2 uvs[4] = vec2[](
        vec2(0.0f, 0.0f), // Bottom-left
        vec2(1.0f, 0.0f), // Bottom-right
        vec2(1.0f, 1.0f), // Top-right
        vec2(0.0f, 1.0f)  // Top-left
    );
    const int indices[6] = int[](0, 1, 2, 0, 2, 3); // For drawing 6 vertices

    gl_Position = vec4(positions[indices[gl_VertexIndex]], 0.0f, 1.0f);
    outUV = uvs[indices[gl_VertexIndex]];
}