#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 pixel_color;
layout(set = 0, binding = 0, std430) readonly buffer PixelColors { vec4 pixel_colors[]; };

const uvec2 screen_size = uvec2(1920u, 1080u);


void main() {
    const uint coord_x = uint(uv.x * screen_size.x);
    const uint coord_y = uint(uv.y * screen_size.y);
    const uint index = coord_x + coord_y * screen_size.x;
    pixel_color = pixel_colors[index];
}
