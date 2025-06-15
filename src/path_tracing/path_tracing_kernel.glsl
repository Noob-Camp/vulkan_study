struct Onb {
    vec3 tangent;
    vec3 binormal;
    vec3 normal;
};

vec3 to_world(Onb onb, vec3 v) {
    return v.x * onb.tangent
         + v.y * onb.binormal
         + v.z * onb.normal;
}


struct Ray {
    vec3 origin;
    vec3 direction;
    float t_min;
    float t_max;
}

Ray make_ray(vec3 origin, vec3 direction, float t_min, float t_max) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;
    ray.t_min = t_min;
    ray.t_max = t_max;
    return ray;
}

vec3 offset_ray_origin(vec3 p, vec3 n) noexcept {
    ivec3 of_i(256.0f * n, 256.0f * n, 256.0f * n);
    auto p_i = as<float3>(as<int3>(p) + ite(p < 0.0f, -of_i, of_i));
    return ite(abs(p) < 1.0f / 32.0f, p + 1.0f / 65536.0f * n, p_i);
}


struct Camera {
    vec3 position;
    vec3 front;
    vec3 up;
    vec3 right;
    float fov;
    vec2 resolution;
};

Ray generate_ray(vec2 p) const noexcept {
    float fov_radians = radians(fov);
    float aspect_ratio = resolution.x / resolution.y;
    vec3 wi_local(
        p.x * tan(0.5f * fov_radians) * aspect_ratio,
        p.y * tan(0.5f * fov_radians),
        -1.0f
    );
    vec3 wi_world = normalize(wi_local.x * right + wi_local.y * up - wi_local.z * front);

    return make_ray(position, wi_world, 0.0f, 100000.0f);
}




uint tea(uint v0, uint v1) {
    uint s0 = 0u;
    for (uint n = 0u; n < 4u; ++n) {
        s0 += 0x9e3779b9u;
        v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
        v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
    }
    return v0;
};

float lcg(uint &state) {
    state = 1664525u * state + 1013904223u;
    return float(state & 0x00ffffffu) * (1.0f / float(0x01000000u));
};

vec3 cosine_sample_hemisphere(vec2 u) {
    float r = sqrt(u.x);
    float phi = 2.0f * constants::pi * u.y;
    return vec3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
};

float balanced_heuristic(float pdf_a, float pdf_b) {
    return pdf_a / max(pdf_a + pdf_b, 1e-4f);
};




layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(push_constant) uniform PushConstants {
    uvec2 screen_size;
    uint hittableCount;
    uint sample_start;
    uint samples;
    uint total_samples;
    uint max_depth;

} push_constants;

layout(set = 0, binding = 0) uniform dispatch_data {
    uint spp_per_dispatch;
    uint depth_per_tracing;
};
layout(set = 0, binding = 1, rgba32f, std140) uniform image2D output_image;
layout(set = 0, binding = 2, r32ui, std140) uniform image2D seed_image;

layout(set = 1, binding = 0, std430) buffer vertices { vec3 vertices[]; }
layout(set = 1, binding = 1, std430) buffer Triangles { vec3 triangles[]; }


void main() {
    vec3 light_emission(17.0f, 12.0f, 4.0f);
    vec3 light_position(-0.24f, 1.98f, 0.16f);
    vec3 light_u = vec3(-0.24f, 1.98f, -0.22f) - light_position;
    vec3 light_v = vec3(0.23f, 1.98f, 0.16f) - light_position;

    uvec2 coord = gl_GlobalInvocationID.xy;
    uint state = imageLoad(seed_image, coord).x;

    float rx = lcg(state);
    float ry = lcg(state);
    vec2 pixel(
        (float(coord.x) + rx) / float(push_constants.screen_size.x) * 2.0f - 1.0f,
        (float(coord.y) + ry) / float(push_constants.screen_size.y) * 2.0f - 1.0f
    );

    vec3 radiance(0.0f, 0.0f, 0.0f);
    for (uint i = 0; i < spp_per_dispatch; ++i) {
        Ray ray = camera.generate_ray(pixel * vec2(1.0f, -1.0f));
        vec3 beta(1.0f, 1.0f, 1.0f);
        float pdf_bsdf = 0.0f;
        float light_area = length(cross(light_u, light_v));
        vec3 light_normal = normalize(cross(light_u, light_v));
        for (uint depth = 0; depth < depth_per_tracing; ++depth) {
            Var<SurfaceHit> hit = accel.intersect(ray, {});
            reorder_shader_execution();
            if (hit.miss()) { break; };
            Triangle triangle = heap.buffer<Triangle>(hit.inst).read(hit.prim);
            vec3 p0 = vertex_buffer.read(triangle.i0);
            vec3 p1 = vertex_buffer.read(triangle.i1);
            vec3 p2 = vertex_buffer.read(triangle.i2);
            vec3 p = triangle_interpolate(hit.bary, p0, p1, p2);
            vec3 n = normalize(cross(p1 - p0, p2 - p0));
            float cos_wo = dot(-ray.direction, n);
            if (cos_wo < 1e-4f) { break; };

            if (hit.inst == uint(meshes.size() - 1u)) {
                if (depth == 0u) {
                    radiance += light_emission;
                } else {
                    float pdf_light = length_squared(p - ray.origin) / (light_area * cos_wo);
                    float mis_weight = balanced_heuristic(pdf_bsdf, pdf_light);
                    radiance += mis_weight * beta * light_emission;
                };
                break;
            };

            float ux_light = lcg(state);
            float uy_light = lcg(state);
            vec3 p_light = light_position + ux_light * light_u + uy_light * light_v;
            vec3 pp = offset_ray_origin(p, n);
            vec3 pp_light = offset_ray_origin(p_light, light_normal);
            float d_light = distance(pp, pp_light);
            vec3 wi_light = normalize(pp_light - pp);
            Ray shadow_ray = make_ray(offset_ray_origin(pp, n), wi_light, 0.0f, d_light);
            bool occluded = accel.intersect_any(shadow_ray, {});
            float cos_wi_light = dot(wi_light, n);
            float cos_light = -dot(light_normal, wi_light);
            vec3 albedo = materials.read(hit.inst);
            if ((!occluded) && (cos_wi_light > 1e-4f) && (cos_light > 1e-4f)) {
                float pdf_light = (d_light * d_light) / (light_area * cos_light);
                float pdf_bsdf = cos_wi_light * inv_pi;
                float mis_weight = balanced_heuristic(pdf_light, pdf_bsdf);
                vec3 bsdf = albedo * inv_pi * cos_wi_light;
                radiance += beta * bsdf * mis_weight * light_emission / max(pdf_light, 1e-4f);
            };

            Onb onb = make_onb(n);
            float ux = lcg(state);
            float uy = lcg(state);
            vec3 wi_local = cosine_sample_hemisphere(vec2(ux, uy));
            float cos_wi = abs(wi_local.z);
            vec3 new_direction = onb->to_world(wi_local);
            ray = make_ray(pp, new_direction);
            pdf_bsdf = cos_wi * inv_pi;
            beta *= albedo;

            float l = dot(make_float3(0.212671f, 0.715160f, 0.072169f), beta);
            if (l == 0.0f) { break; };
            float q = max(l, 0.05f);
            float r = lcg(state);
            if (r >= q) { break; };
            beta *= 1.0f / q;
        };
    };

    radiance /= float(spp_per_dispatch);
    seed_image.write(coord, make_uint4(state));
    if (isnan(radiance)) { radiance = vec3(0.0f, 0.0f, 0.0f); };

    image.write(coord, vec4(clamp(radiance, 0.0f, 30.0f), 1.0f));
};
