#version 450

layout(location = 0) in vec2 inUV;
layout(set = 0, binding = 0, std430) readonly buffer ImageOutputSSBO {
    vec4 pixel_colors[]; // The array of pixel colors
} output_pixel_buffer;
layout(location = 0) out vec4 outColor; // Output color to the framebuffer

const vec2 screen_size = vec2(800.0, 600.0);


void main() {
    ivec2 pixel_coords = ivec2(inUV.x * screen_size.x, inUV.y * screen_size.y);
    uint output_index = uint(pixel_coords.y) * uint(screen_size.x) + uint(pixel_coords.x);
    if (output_index < screen_size.x * screen_size.y) {
        outColor = output_pixel_buffer.pixel_colors[output_index];
    } else {
        outColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta for error visibility
    }
}
