#pragma once

#include <glm/vec3.hpp>


using point3 = glm::vec3;
using vec3 = glm::vec3;


struct Camera {
    alignas(16) glm::vec3 origin;
    alignas(16) glm::vec3 horizontal;
    alignas(16) glm::vec3 vertical;
    alignas(16) glm::vec3 lowerLeftCorner;
    float viewportHeight { 0.0 };
    float viewportWidth { 0.0 };
    float aspectRatio { 0.0 };
    float focalLength { 0.0 };
    alignas(16) glm::vec3 u;
    alignas(16) glm::vec3 v;
    alignas(16) glm::vec3 w;
    float lensRadius { 0.0 };


    Camera() = default;
    Camera(
        point3 lookfrom, point3 lookat, vec3 vup,
        float vfov, // vertical field-of-view in degrees
        float aspectRatio,
        float aperture,
        float focus_dist
    ) {
        // const auto offset = offsetof(Camera, viewportHeight);
        float tan = glm::tan(glm::radians(vfov) / 2);
        viewportHeight = 2.0f * tan * focus_dist;
        viewportWidth = aspectRatio * viewportHeight;
        this->aspectRatio = aspectRatio;

        w = glm::normalize(lookfrom - lookat);
        u = glm::normalize(cross(vup, w));
        v = glm::cross(w, u);

        origin = lookfrom;
        horizontal = viewportWidth * u;
        vertical = viewportHeight * v;
        lowerLeftCorner = origin - horizontal / 2.0f - vertical / 2.0f - w * focus_dist;
        focalLength = focus_dist;
        lensRadius = aperture / 2.0f;
    }
};
