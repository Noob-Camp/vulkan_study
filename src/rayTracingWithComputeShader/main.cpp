#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <future>
#include <thread>
#include <chrono>

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>

#include "minilog.hpp"
#include "constantData.hpp"
#include "camera.hpp"
#include "image.hpp"
#include "materials/material.hpp"
#include "dataDump.h"

using namespace std::literals::string_literals;


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };


struct RayTracingWithComputeShader {
    uint32_t width { 800u };
    uint32_t height { 600u };

    vk::Instance instance { nullptr };
    bool validationLayersSupported = false;
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    vk::DebugUtilsMessengerEXT debugMessenger { nullptr };

    vk::PhysicalDevice physicalDevice { nullptr };
    vk::Device logicalDevice { nullptr };
    vk::Queue computeQueue { nullptr };
    std::optional<uint32_t> computeQueueFamilyIndex;

    std::vector<vk::Buffer> storageBuffers;
    std::vector<vk::DeviceMemory> storageBufferMemorys;
    vk::Buffer uniformBuffer;
    vk::DeviceMemory uniformBufferMemory;

    vk::DescriptorPool descriptorPool;
    std::array<vk::DescriptorSetLayout, 2> descriptorSetLayouts;
    std::vector<vk::DescriptorSet> descriptorSets;
    vk::PipelineLayout pipelineLayout;
	vk::Pipeline computePipeline;

	vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> computeCommandBuffer;

    // ray tracing begin
    uint32_t computeShaderProcessUnit;
    Camera camera;
    glm::ivec2 screenSize;
    PushConstantData pushConstantData;
    Image target;
    HittableDump hittables;
    MaterialDump materials;
    const std::size_t maxSamplesForSingleShader = 50;

    RayTracingWithComputeShader(
        const uint32_t& w,
        const uint32_t& h
    )
        : width(w), height(h)
    {}

    ~RayTracingWithComputeShader() { cleanUp(); }

    void run() {
        initVulkan();
        initCompute();
    }

    void initVulkan() {
        checkValidationLayerSupport();
        populateDebugUtilsMessengerCreateInfoEXT();
        createInstance();
        setupDebugMessenger();

        pickPhysicalDevice();
        createLogicalDevice();
    }

    void initCompute() {
        createScene();

        createBuffers();
        writeMemoryFromHost();
        createDescriptorSetLayout();
        createComputePipeline();

        createDescriptorPool();
        createDescriptorSet();
        createCommandPool();

        execute();
        output();
    }

    void cleanUp() {
        logicalDevice.destroyCommandPool(commandPool);
        logicalDevice.destroyDescriptorPool(descriptorPool, nullptr);
        logicalDevice.destroyPipelineLayout(pipelineLayout);
        logicalDevice.destroyPipeline(computePipeline);

        for (vk::DescriptorSetLayout descriptorSetLayout : descriptorSetLayouts) {
            logicalDevice.destroyDescriptorSetLayout(descriptorSetLayout);
        }
        for (vk::Buffer buffer : storageBuffers) {
            logicalDevice.destroyBuffer(buffer);
        }
        logicalDevice.destroyBuffer(uniformBuffer);
        for (vk::DeviceMemory bufferMemory : storageBufferMemorys) {
            logicalDevice.freeMemory(bufferMemory);
        }
        logicalDevice.freeMemory(uniformBufferMemory);

        logicalDevice.waitIdle();
        logicalDevice.destroy();

        vk::detail::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
        instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, dispatch);

        instance.destroy();

        minilog::log_info("the compute shader programme is destruction.");
    }

    VKAPI_ATTR VKAPI_CALL
    static vk::Bool32 debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) { return; }
        vk::detail::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::detail::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
        if (instance.createDebugUtilsMessengerEXT(
            &debugCreateInfo, nullptr, &debugMessenger, dispatch) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to set up debug messenger!");
        }
    }

    void checkValidationLayerSupport() {
        std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
        for (const char* layerName : validationLayers) {
            for (const vk::LayerProperties& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    validationLayersSupported = true;
                    minilog::log_info("the {} is supported!", layerName);
                    break;
                }
            }
            if (validationLayersSupported) { break; }
        }

        if (enableValidationLayers && (!validationLayersSupported)) {
            minilog::log_fatal("validation layers requested, but not available!");
        }
    }

    void populateDebugUtilsMessengerCreateInfoEXT() {
        debugCreateInfo.flags = vk::DebugUtilsMessengerCreateFlagsEXT();
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback);
        debugCreateInfo.pUserData = nullptr;// optional
    }

    void createInstance() {
        vk::ApplicationInfo appInfo {
            .pApplicationName = "Ray Tracing with compute shader",
            .applicationVersion = VK_MAKE_VERSION(1, 3, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 3, 0),
            .apiVersion = VK_API_VERSION_1_3
        };

        vk::InstanceCreateInfo instanceCreateInfo {
            .pApplicationInfo = &appInfo
        };

        if (enableValidationLayers) {
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            instanceCreateInfo.enabledLayerCount = 0u;
            instanceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        std::vector<const char*> extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        if (vk::createInstance(&instanceCreateInfo, nullptr, &instance) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::Instance!");
        } else {
            minilog::log_info("create vk::Instance successfully!");
        }
    }


    void pickPhysicalDevice() {
        std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        for (const vk::PhysicalDevice& device : devices) {
            std::vector<vk::QueueFamilyProperties> queueFamilies =
                device.getQueueFamilyProperties();
            for (std::size_t i = 0; i < queueFamilies.size(); ++i) {
                if (queueFamilies[i].queueFlags & (vk::QueueFlagBits::eCompute)) {
                    computeQueueFamilyIndex = i;
                    physicalDevice = device;
                    break;
                }
            }
            if (computeQueueFamilyIndex.has_value()) break;
        }

        if (physicalDevice == nullptr) {
            minilog::log_fatal("failed to find a suitable GPU!");
        }

        vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
        minilog::log_info("maxComputeWorkGroupInvocations:", properties.limits.maxComputeWorkGroupInvocations);
        computeShaderProcessUnit = sqrt(properties.limits.maxComputeWorkGroupInvocations);
    }

    void createLogicalDevice() {
        float queuePriority { 1.0f };
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        vk::DeviceQueueCreateInfo queueCreateInfo {
            .queueFamilyIndex = computeQueueFamilyIndex.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);

        vk::DeviceCreateInfo deviceCreateInfo {
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data()
        };

        if (enableValidationLayers) {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            deviceCreateInfo.enabledLayerCount = 0;
            deviceCreateInfo.ppEnabledLayerNames = nullptr;
        }

        deviceCreateInfo.enabledExtensionCount = 0u;
        deviceCreateInfo.ppEnabledExtensionNames = nullptr;

        vk::PhysicalDeviceFeatures physicalDeviceFeatures {};
        physicalDeviceFeatures.samplerAnisotropy = vk::Bool32(VK_TRUE);
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        if (physicalDevice.createDevice(&deviceCreateInfo, nullptr, &logicalDevice) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create logical device!");
        } else {
            minilog::log_info("create logical device successfully!");
        }

        logicalDevice.getQueue(computeQueueFamilyIndex.value(), 0, &computeQueue);
    }

    static std::vector<char> readFile(const std::string& fileName) {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            minilog::log_fatal("failed to open file: {}", fileName);
        }

        std::size_t fileSize = static_cast<std::size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo {
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        vk::ShaderModule shaderModule;
        if (logicalDevice.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::ShaderModule");
        } else {
            minilog::log_info("create vk::ShaderModule successfully!");
        }

        return shaderModule;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount { 0u };
        const char** glfwExtensions = nullptr;
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void createScene() {
        // image
        const auto aspectRatio =
            static_cast<float>(width) / static_cast<float>(height);

        pushConstantData.screenSize = { width, height };
        pushConstantData.maxDepth = 50;
        pushConstantData.totalSamples = 200;

        target = Image(width, height);
        target.gammaCorrectOnOutput = true;

        auto groundMaterial = materials.Allocate<Lambertian>(color(249.0 / 255.0, 189.0 / 255.0, 219.0 / 255.0));
        hittables.Allocate<Sphere>(point3(0, -1000, 0), 1000)->mat = groundMaterial;

        for (int a = -11; a < 11; a++) {
            for (int b = -11; b < 11; b++) {
                using namespace glm;
                auto choose_mat = linearRand(0.0, 1.0);
                point3 center(a + 0.9 * linearRand(0.0, 1.0), 0.2, b + 0.9 * linearRand(0.0, 1.0));

                if (distance(center, point3(4, 0.2, 0)) > 0.9) {
                    Material* mat;
                    auto percent = 0.0;
                    //orange
                    if (choose_mat / 0.7 < (percent += 0.2)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(254.0 / 255.0, 193.0 / 255.0, 172.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    //purple
                    else if (choose_mat / 0.7 < (percent += 0.15)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(249.0 / 255.0, 205.0 / 255.0, 255.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    //blue
                    else if (choose_mat / 0.7 < (percent += 0.20)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(187.0 / 255.0, 240.0 / 255.0, 239.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    //dark blue
                    else if (choose_mat / 0.7 < (percent += 0.10)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(185.0 / 255.0, 203.0 / 255.0, 255.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    //green
                    else if (choose_mat / 0.7 < (percent += 0.15)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(197.0 / 255.0, 243.0 / 255.0, 195.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    //yellow
                    else if (choose_mat / 0.7 < (percent += 0.20)) {
                        // diffuse
                        auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                            + vec3(245.0 / 255.0, 241.0 / 255.0, 185.0 / 255.0);
                        mat = materials.Allocate<Lambertian>(albedo);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    ////red
                    //else if (choose_mat / 0.8 < (percent += 0.10)) {
                    //	// diffuse
                    //	auto albedo = linearRand(vec3(-0.1), vec3(0.1)) * linearRand(vec3(-0.1), vec3(0.1))
                    //		+ vec3(251.0 / 255.0, 197.0 / 255.0, 201.0 / 255.0);
                    //	mat = materials.Allocate<Lambertian>(albedo);
                    //	hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    //}
                    else if (choose_mat < 0.9) {
                        // metal
                        auto albedo = linearRand(vec3(0.5), vec3(1));
                        auto fuzz = linearRand(0.0, 0.5);
                        mat = materials.Allocate<Metal>(albedo, fuzz);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                    else {
                        //glass
                        mat = materials.Allocate<Dielectric>(1.5);
                        hittables.Allocate<Sphere>(center, 0.2)->mat = mat;
                    }
                }
            }
        }

        //create materials
        auto material1 = materials.Allocate<Dielectric>(1.5);
        hittables.Allocate<Sphere>(point3(0, 1, 0), 1.0)->mat = material1;

        auto material2 = materials.Allocate<Lambertian>(color(242.0 / 255.0, 220.0 / 255.0, 196.0 / 255.0));
        hittables.Allocate<Sphere>(point3(-4, 1, 0), 1.0)->mat = material2;

        auto material3 = materials.Allocate<Metal>(color(253.0 / 255.0, 236.0 / 255.0, 223.0 / 255.0), 0.0);
        hittables.Allocate<Sphere>(point3(4, 1, 0), 1.0)->mat = material3;

        // Camera
        point3 lookfrom(13, 2, 3);
        point3 lookat(0, 0, 0);
        vec3 vup(0, 1, 0);
        auto distTofocus = 5.0;
        auto aperture = 0;
        camera = Camera(lookfrom, lookat, vup, 30, aspectRatio, aperture, distTofocus);

        pushConstantData.hittableCount = hittables.Count();
    }

    void createBuffer(vk::BufferUsageFlags usage, vk::Buffer& buffer, vk::DeviceMemory& memory) {
        vk::BufferCreateInfo createInfo {
            .pNext = nullptr,
            .size = target.imageSize(),
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &computeQueueFamilyIndex.value()
        };
        if (logicalDevice.createBuffer(&createInfo, nullptr, &buffer) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create vk::buffer!");
        }

        vk::MemoryRequirements requirements = logicalDevice.getBufferMemoryRequirements(buffer);

        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = requirements.size,
            .memoryTypeIndex = findMemoryType(requirements,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
        };

        if (logicalDevice.allocateMemory(&allocInfo, nullptr, &memory) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to allocate buffer memory!");
        }

        logicalDevice.bindBufferMemory(buffer, memory, 0);
    }

    void createBuffers() {
        storageBuffers.resize(5);
        storageBufferMemorys.resize(5);
        for (std::size_t i = 0; i < 5uz; i++) {
            createBuffer(
                vk::BufferUsageFlagBits::eStorageBuffer,
                storageBuffers[i],
                storageBufferMemorys[i]
            );
        }
        createBuffer(vk::BufferUsageFlagBits::eUniformBuffer, uniformBuffer, uniformBufferMemory);
    }

    uint32_t findMemoryType(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if (requirements.memoryTypeBits & (1 << i) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                minilog::log_info("pick memory type [{0}]", i);
                return i;
            }
        }
    }

    void WriteMemory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data = logicalDevice.mapMemory(memory, 0, size, {});
        memcpy(data, dataBlock, size);
        logicalDevice.unmapMemory(memory);
    }

    void writeMemoryFromHost() {
        WriteMemory(storageBufferMemorys[0], target.imageData.data(), target.imageSize());
        materials.Dump();
        hittables.Dump();
        materials.WriteMemory(logicalDevice, storageBufferMemorys[1], storageBufferMemorys[2]);
        hittables.WriteMemory(logicalDevice, storageBufferMemorys[3], storageBufferMemorys[4]);
        WriteMemory(uniformBufferMemory, &camera, sizeof(camera));
    }

    void createDescriptorSetLayout() {
        {
            std::array<vk::DescriptorSetLayoutBinding, 5> bindings;
            for (std::size_t i = 0; i < bindings.size(); i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = vk::DescriptorType::eStorageBuffer;
                bindings[i].pImmutableSamplers = nullptr;
                bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
            }

            vk::DescriptorSetLayoutCreateInfo createInfo {
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()
            };
            if (logicalDevice.createDescriptorSetLayout(
                &createInfo, nullptr, &descriptorSetLayouts[0]
                ) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create descriptorSetLayout!");
            }
        }

        {
            std::array<vk::DescriptorSetLayoutBinding, 1> bindings;
            for (std::size_t i = 0; i < bindings.size(); i++) {
                bindings[i].binding = 0;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = vk::DescriptorType::eUniformBuffer;
                bindings[i].pImmutableSamplers = nullptr;
                bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
            }

            vk::DescriptorSetLayoutCreateInfo createInfo {
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()
            };

            if (logicalDevice.createDescriptorSetLayout(
                &createInfo, nullptr, &descriptorSetLayouts[1]
                ) != vk::Result::eSuccess
            ) {
                minilog::log_fatal("failed to create descriptorSetLayout!");
            }
        }
    }

	void createComputePipeline() {
        std::vector<char> computeShaderCode =
            readFile("../../src/rayTracingWithComputeShader/shaders/raytracing/noSamples.spv");
		vk::ShaderModule computeShaderModule = createShaderModule(computeShaderCode);

		vk::PipelineShaderStageCreateInfo shaderStageCreateInfo {
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = computeShaderModule,
            .pName = "main"
        };

		vk::SpecializationMapEntry entry {
            .constantID = 0,
            .offset = 0,
            .size = sizeof(uint32_t)
        };

		vk::SpecializationInfo specInfo {
            .mapEntryCount = 1,
            .pMapEntries = &entry,
            .dataSize = sizeof(computeShaderProcessUnit),
            .pData = &computeShaderProcessUnit
        };

        shaderStageCreateInfo.pSpecializationInfo = &specInfo;

		vk::PushConstantRange range {
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .offset = 0,
            .size = sizeof(PushConstantData)
        };

		vk::PipelineLayoutCreateInfo layoutCreateInfo {
            .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range
        };
        pipelineLayout = logicalDevice.createPipelineLayout(layoutCreateInfo);

		vk::ComputePipelineCreateInfo createInfo {
            .stage = shaderStageCreateInfo,
            .layout = pipelineLayout,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (logicalDevice.createComputePipelines(
            nullptr, 1, &createInfo, nullptr, &computePipeline) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create compute vk::Pipeline!");
        } else {
            minilog::log_info("create compute vk::Pipeline successfully!");
        }

        logicalDevice.destroyShaderModule(computeShaderModule, nullptr);
    }

    void createDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 2> poolSize;
        poolSize[0].descriptorCount = 1 + 2 + 2;
        poolSize[0].type = vk::DescriptorType::eStorageBuffer;
        poolSize[1].descriptorCount = 1;
        poolSize[1].type = vk::DescriptorType::eUniformBuffer;

        vk::DescriptorPoolCreateInfo createInfo {
            .maxSets = 2,
            .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
            .pPoolSizes = poolSize.data()
        };

        if (logicalDevice.createDescriptorPool(&createInfo, nullptr, &descriptorPool)
                != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptor pool!");
        }
    }

	void createDescriptorSet() {
		vk::DescriptorSetAllocateInfo allocInfo {
            .descriptorPool = descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.data()
        };

        descriptorSets = logicalDevice.allocateDescriptorSets(allocInfo);

        std::array<vk::DescriptorBufferInfo, 5> storageBufferInfos;
        for (std::size_t i = 0; i < storageBufferInfos.size(); i++) {
            storageBufferInfos[i].buffer = storageBuffers[i];
            storageBufferInfos[i].offset = 0;
        }
        storageBufferInfos[0].range = target.imageSize();
        storageBufferInfos[1].range = materials.HeadSize();
        storageBufferInfos[2].range = materials.DumpSize();
        storageBufferInfos[3].range = hittables.HeadSize();
        storageBufferInfos[4].range = hittables.DumpSize();

        vk::DescriptorBufferInfo uniformBufferInfo;
        uniformBufferInfo.buffer = uniformBuffer;
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(camera);

        vk::WriteDescriptorSet write {};
        std::array<vk::WriteDescriptorSet, 6> writes;
        writes.fill(write);
        //for storage buffers
        for (std::size_t i = 0; i < writes.size() - 1; ++i) {
            writes[i].descriptorCount = 1;
            writes[i].dstSet = descriptorSets[0];
            writes[i].dstArrayElement = 0;
            writes[i].dstBinding = i;
            writes[i].descriptorType = vk::DescriptorType::eStorageBuffer;
            writes[i].pBufferInfo = &storageBufferInfos[i];
        }
        //for camera uniform buffer
        writes[5].descriptorCount = 1;
        writes[5].dstSet = descriptorSets[1];
        writes[5].dstArrayElement = 0;
        writes[5].dstBinding = 0;
        writes[5].descriptorType = vk::DescriptorType::eUniformBuffer;
        writes[5].pBufferInfo = &uniformBufferInfo;

        logicalDevice.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void createCommandPool() {
        vk::CommandPoolCreateInfo createInfo {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = computeQueueFamilyIndex.value()
        };

        if (logicalDevice.createCommandPool(&createInfo, nullptr, &commandPool)
            != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create command pool!");
        }
    }

    void createCommandBufferCompute() {
        computeCommandBuffer.resize(1);
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        if (logicalDevice.allocateCommandBuffers(&allocInfo, computeCommandBuffer.data()) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create command buffer!");
        }
    }

    void execute(uint32_t sampleStart, uint32_t samples) {
        computeCommandBuffer[0].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
        auto start = std::chrono::high_resolution_clock::now();
        vk::CommandBufferBeginInfo beginInfo {};
        computeCommandBuffer[0].begin(&beginInfo);
        computeCommandBuffer[0].bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);

        computeCommandBuffer[0].bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            pipelineLayout,
            0,
            static_cast<uint32_t>(descriptorSets.size()),
            descriptorSets.data(),
            0,
            nullptr
        );

        pushConstantData.samples = samples;
        pushConstantData.sampleStart = sampleStart;

        computeCommandBuffer[0].pushConstants(
            pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(pushConstantData), &pushConstantData
        );

        auto sizeX = static_cast<uint32_t>(target.width / computeShaderProcessUnit + 1);
        auto sizeY = static_cast<uint32_t>(target.height / computeShaderProcessUnit + 1);
        minilog::log_info("WorkX = {0}, WorkY = {1}", sizeX, sizeY);
        computeCommandBuffer[0].dispatch(16, 16, 1);
        computeCommandBuffer[0].end();

        vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = &computeCommandBuffer[0],
            .signalSemaphoreCount = 0
        };

        if (computeQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit command buffer!");
        }

        //wait the calculation to finish
        computeQueue.waitIdle();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> delta = end - start;
        minilog::log_info(
            "[{0}/{1}] GPU Process Time: {2}s",
            sampleStart + samples,
            pushConstantData.totalSamples,
            delta.count()
        );
    }

    void execute() {
        createCommandBufferCompute();
        auto restSamples = pushConstantData.totalSamples;
        auto sampleStart = 0;
        while (true) {
            if (restSamples >= maxSamplesForSingleShader) {
                execute(sampleStart, maxSamplesForSingleShader);
                restSamples -= maxSamplesForSingleShader;
                sampleStart += maxSamplesForSingleShader;
            } else if (restSamples > 0) {
                execute(sampleStart, restSamples);
                restSamples = 0;
            } else {
                minilog::log_info("total: {0}\nDone!", pushConstantData.totalSamples);
                break;
            }
        }
    }

	void ReadMemory(vk::DeviceMemory memory, void* dataBlock, vk::DeviceSize size) {
        void* data = logicalDevice.mapMemory(memory, 0, size, {});
        memcpy(dataBlock, data, size);
        logicalDevice.unmapMemory(memory);
	}

    bool finish = false;
    void output() {
        ReadMemory(storageBufferMemorys[0], target.imageData.data(), target.imageSize());
        std::filesystem::path out("./RenderingTarget.ppm");
        auto absPath = std::filesystem::absolute(out);
        std::cout << "Output Path: " << absPath << "\n";
        minilog::log_info("Wait:\n");
        std::future<void> f = std::async(
            [this] () {
                while (!finish) {
                    std::chrono::duration<double, std::milli> time(500);
                    for (auto i = 0; i < 3; ++i) {
                        std::this_thread::sleep_for(time);
                        switch (i) {
                            case 0: std::cout << "\r/" << std::flush; break;
                            case 1: std::cout << "\r-" << std::flush; break;
                            case 2: std::cout << "\r\\" << std::flush; break;
                            default:
                                break;
                        }
                    }
                }
            }
        );
        std::ofstream os(out);
        os << target;
        finish = true;
        f.wait();
        minilog::log_info("\nOutput Finished!");
    }
};


int main(int argc, const char* argv[]) {
    RayTracingWithComputeShader app { 800u, 600u };

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
