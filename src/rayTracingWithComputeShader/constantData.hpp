#pragma once

#include <glm/glm.hpp>


struct PushConstantData {
    glm::ivec2 screenSize { 0, 0 };
    uint32_t hittableCount { 0u };
    uint32_t sampleStart { 0u };
    uint32_t samples { 0u };
    uint32_t totalSamples { 0u };
    uint32_t maxDepth { 0u };
};
