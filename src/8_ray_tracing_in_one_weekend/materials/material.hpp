#pragma once

#include <glm/glm.hpp>


template<typename Ty, typename GlslTy, typename DumpTy>
class DataDump {
protected:
    std::vector<Ty*> handles;

public:
    std::vector<GlslTy> heads;
    std::vector<DumpTy> dump;

    DataDump() = default;

    template<typename Detrive, typename ...Args>
    Detrive* Allocate(Args&&... args) {
        static_assert(std::is_base_of_v<Ty, Detrive>, "need to derive from Ty");
        auto ret = new Detrive(std::forward<Args>(args)...);
        ret->ptr = handles.size();
        handles.push_back(ret);
        return ret;
    }

    void Clear() {
        for (auto handle : handles) delete handle;
        handles.clear();
    }

    ~DataDump() { Clear(); }

    uint32_t HeadSize() { return heads.size() * sizeof(GlslTy); }

    uint32_t DumpSize() { return dump.size() * sizeof(DumpTy); }

    void WriteMemory(
        vk::Device device,
        vk::DeviceMemory headMemory, vk::DeviceMemory dumpMemory
    ) {
        WriteMemory(device, headMemory, heads.data(), HeadSize());
        WriteMemory(device, dumpMemory, dump.data(), DumpSize());
    }

private:
    void WriteMemory(
        vk::Device device,
        vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size
    ) {
        void* data = device.mapMemory(memory, 0, size, {});
        memcpy(data, dataBlock, size);
        device.unmapMemory(memory);
    }
};




enum class MaterialType : uint32_t {
    None = 0,
    Lambertian = 1,
    Metal = 2,
    Dielectrics = 3
};


struct GLSLMaterial {
    MaterialType type = MaterialType::None;
    uint32_t ptr;
};


class Material {
    friend class MaterialDump;
    friend class HittableDump;
    friend class DataDump<Material, GLSLMaterial, glm::vec4>;
public:
    MaterialType type;
    virtual std::vector<glm::vec4> Dump() = 0;
private:
    uint32_t ptr;
};




struct Lambertian : public Material {
    glm::vec3 albedo { 0.0, 0.0, 0.0 };

    Lambertian(const glm::vec3 albedo)
        :albedo(albedo)
    {
        this->type = MaterialType::Lambertian;
    }

    virtual std::vector<glm::vec4> Dump() override {
        return { glm::vec4(albedo, 0.0) };
    }
};

struct Metal :public Material {
    glm::vec3 albedo { 0.0, 0.0, 0.0 };
    float fuzz;

    Metal(const glm::vec3& a, float fuzz)
        :albedo(a)
        , fuzz(fuzz < 1 ? fuzz : 1)
    {
        this->type = MaterialType::Metal;
    };

    virtual std::vector<glm::vec4> Dump() override {
        return { glm::vec4(albedo, fuzz) };
    }
};

struct Dielectric :public Material {
    float ir;

    Dielectric(float ir) : ir(ir) {
        this->type = MaterialType::Dielectrics;
    }

    virtual std::vector<glm::vec4> Dump() override {
        return { glm::vec4(ir, 0, 0, 0) };
    }
};



struct MaterialDump: public DataDump<Material, GLSLMaterial, glm::vec4> {
    void Dump() {
        heads.clear();
        heads.reserve(handles.size());

        for (auto handle : handles) {
            heads.push_back({ handle->type,handle->ptr });
            auto vec4s = handle->Dump();
            for (const auto& v : vec4s) dump.push_back(v);
        }
    }
};
