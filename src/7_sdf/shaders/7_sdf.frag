#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 pixel_color;
layout(set = 0, binding = 3, std430) readonly buffer PixelColors { vec4 pixel_colors[]; };

const uvec2 screen_size = uvec2(1920u, 1080u);
const float hdr2ldr_scale = 2.0f;


vec3 linear_to_srgb(vec3 x) {
    vec3 srgb_result = mix(
        1.055f * pow(x, vec3(1.0f / 2.4f)) - 0.055f,
        12.92f * x,
        lessThanEqual(x, vec3(0.00031308f))
    );

    return clamp(srgb_result, 0.0f, 1.0f);
}


void main() {
    const uint coord_x = uint(uv.x * screen_size.x);
    const uint coord_y = uint(uv.y * screen_size.y);
    const uint index = coord_x + coord_y * screen_size.x;

    if (index < screen_size.x * screen_size.y) {
        pixel_color = pixel_colors[index];
    } else {
        pixel_color = vec4(1.0f, 0.55f, 0.55f, 1.0);
    }

    pixel_color = vec4(
        linear_to_srgb(
            clamp(
                pixel_color.xyz / pixel_color.w * hdr2ldr_scale,
                0.0f,
                1.0f
            )
        ),
        pixel_color.w
    );
}
