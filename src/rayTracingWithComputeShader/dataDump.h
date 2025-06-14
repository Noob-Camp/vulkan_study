#pragma once


#include <vector>
#include <glm/glm.hpp>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>


using color = glm::vec3;
using point3 = glm::vec3;
using vec3 = glm::vec3;


struct Material;

template<typename Ty, typename GlslTy, typename DumpTy>
class DataDump;


enum class HittableType : uint32_t
{
    None = 0, TriangleMesh, Sphere
};

struct GLSLHittable
{
    HittableType type = HittableType::None;
    uint32_t ptr;
    uint32_t mat = 0;
};

class Hittable
{
    friend class HittableDump;
    friend class DataDump<Hittable, GLSLHittable, glm::vec4>;
public:
    HittableType type;
    virtual std::vector<glm::vec4> Dump() = 0;
    Material* mat;
private:
    uint32_t ptr;
};

class Sphere : public Hittable
{
public:
    Sphere(const glm::vec3& center, float radius)
        :center(center), radius(radius)
    {
        this->type = HittableType::Sphere;
    }
    glm::vec3 center;
    float radius;
    virtual std::vector<glm::vec4> Dump() override
    {
        return { glm::vec4(center, radius) };
    }
};

class HittableDump : public DataDump<Hittable, GLSLHittable, glm::vec4>
{
public:
    uint32_t Count() { return handles.size(); }
    void Dump()
    {
        heads.clear();
        heads.reserve(handles.size());
        for (auto handle : handles)
        {
            heads.push_back({ handle->type, handle->ptr, handle->mat->ptr });
            auto vec4s = handle->Dump();
            for (const auto& v : vec4s) dump.push_back(v);
        }
    }
};