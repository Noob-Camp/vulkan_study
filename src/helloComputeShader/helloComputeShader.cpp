#include <string>
#include <optional>
#include <array>

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include <minilog.hpp>

using namespace std::literals::string_literals;


#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

const std::vector<const char*>
validationLayers = { "VK_LAYER_KHRONOS_validation" };


struct HelloComputeShader {
    vk::Instance instance { nullptr };
    bool validationLayersSupported = false;
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
    vk::DebugUtilsMessengerEXT debugMessenger { nullptr };

    vk::PhysicalDevice physicalDevice { nullptr };
    vk::Device logicalDevice { nullptr };
    vk::Queue computeQueue;
    std::optional<uint32_t> computeQueueFamilyIndex;

    vk::Buffer storageBuffer;
    vk::DeviceMemory storageBufferMemory;

    vk::DescriptorPool descriptorPool;
    vk::DescriptorSetLayout descriptorSetLayout;
    std::vector<vk::DescriptorSet> descriptorSets;

    vk::PipelineLayout pipelineLayout;
    vk::Pipeline computePipeline;

    vk::CommandPool commandPool;
    std::vector<vk::CommandBuffer> commandBuffers;

    std::array<float, 1024> inputData;
    std::array<float, 1024> outputData;


    HelloComputeShader() {
        inputData.fill(1.0f);
        outputData.fill(0.0f);
    }
    ~HelloComputeShader() { cleanUp(); }

    void initVulkan() {
        checkValidationLayerSupport();
        populateDebugUtilsMessengerCreateInfoEXT();
        createInstance();
        setupDebugMessenger();

        pickPhysicalDevice();
        createLogicalDevice();

        createStorageBuffer();
        createDescriptorPool();
        createDescriptorSetLayout();
        createDescriptorSet();
        createComputePipeline();

        createCommandPool();
        createCommandBuffer();

        execute();
    }

    void cleanUp() {
        logicalDevice.destroyCommandPool(commandPool);
        logicalDevice.destroyDescriptorPool(descriptorPool, nullptr);
        logicalDevice.destroyPipelineLayout(pipelineLayout);
        logicalDevice.destroyPipeline(computePipeline);
        logicalDevice.destroyDescriptorSetLayout(descriptorSetLayout);
        logicalDevice.destroyBuffer(storageBuffer);
        logicalDevice.freeMemory(storageBufferMemory);

        logicalDevice.waitIdle();
        logicalDevice.destroy();

        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
        instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, dispatch);

        instance.destroy();
        
        minilog::log_info("the compute shader programme is destruction.");
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

    void populateDebugUtilsMessengerCreateInfoEXT() {
        debugCreateInfo.flags = vk::DebugUtilsMessengerCreateFlagsEXT();
        debugCreateInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugCreateInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                                        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        debugCreateInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(debugCallback);
        debugCreateInfo.pUserData = nullptr;// optional
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount { 0u };
        // const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        const char** glfwExtensions = nullptr;
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    void createInstance() {
        vk::ApplicationInfo appInfo {
            .pApplicationName = "hello compute shader",
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

    void setupDebugMessenger() {
        if (!enableValidationLayers) { return; }
        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        vk::DispatchLoaderDynamic dispatch(instance, getInstanceProcAddr);
        if (instance.createDebugUtilsMessengerEXT(
            &debugCreateInfo, nullptr, &debugMessenger, dispatch
            ) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to set up debug messenger!");
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

    void createStorageBuffer() {
        vk::DeviceSize inputDataSize = sizeof(inputData);
        vk::BufferCreateInfo createInfo {
            .size = inputDataSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
        };

        if (logicalDevice.createBuffer(
            &createInfo, nullptr, &storageBuffer) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::buffer!");
        }

        vk::MemoryRequirements requirements = logicalDevice.getBufferMemoryRequirements(storageBuffer);
        vk::MemoryAllocateInfo allocInfo {
            .allocationSize = requirements.size,
            .memoryTypeIndex =
                findMemoryType(
                    requirements,
                    vk::MemoryPropertyFlagBits::eHostVisible
                    | vk::MemoryPropertyFlagBits::eHostCoherent
                )
        };
        
        if (logicalDevice.allocateMemory(
            &allocInfo, nullptr, &storageBufferMemory) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to allocate buffer memory!");
        }
        logicalDevice.bindBufferMemory(storageBuffer, storageBufferMemory, 0);


        void* data = logicalDevice.mapMemory(storageBufferMemory, 0, sizeof(inputData), {});
        memcpy(data, inputData.data(), sizeof(inputData));
        logicalDevice.unmapMemory(storageBufferMemory);
    }

    uint32_t findMemoryType(const vk::MemoryRequirements& requirements, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if (requirements.memoryTypeBits & (1 << i) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties
            ) {
                std::cout << "pick memory type [" << i << "]\n";
                return i;
            }
        }

        return 0u;
    }

    void createDescriptorPool() {
        vk::DescriptorPoolSize poolSize {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1
        };

        vk::DescriptorPoolCreateInfo createInfo {
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };

        if (logicalDevice.createDescriptorPool(&createInfo, nullptr, &descriptorPool)
                != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptor pool!");
        }
    }

    void createDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding binding {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr
        };

        vk::DescriptorSetLayoutCreateInfo createInfo {
            .bindingCount = 1,
            .pBindings = &binding
        };

        if (logicalDevice.createDescriptorSetLayout(
            &createInfo, nullptr, &descriptorSetLayout
            ) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create descriptorSetLayout!");
        }
    }

    void createDescriptorSet() {
        vk::DescriptorSetAllocateInfo allocInfo {
            .descriptorPool = descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptorSetLayout
        };
        descriptorSets = logicalDevice.allocateDescriptorSets(allocInfo);

        vk::DescriptorBufferInfo bufferInfo {
            .buffer = storageBuffer,
            .offset = 0,
            .range = sizeof(inputData),
        };

        vk::WriteDescriptorSet write {
            .dstSet = descriptorSets[0],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &bufferInfo
        };

        logicalDevice.updateDescriptorSets(1, &write, 0, nullptr);
    }

    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo {
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        vk::ShaderModule shaderModule;
        if (logicalDevice.createShaderModule(
            &createInfo, nullptr, &shaderModule) != vk::Result::eSuccess
        ) {
            minilog::log_fatal("failed to create vk::ShaderModule");
        } else {
            minilog::log_info("create vk::ShaderModule successfully!");
        }

        return shaderModule;
    }

    static std::vector<char> readFile(const std::string& fileName) {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            minilog::log_fatal("failed to open file: {}", fileName);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    void createComputePipeline() {
        std::vector<char> computeShaderCode =
            readFile("../../src/helloComputeShader/test.spv");
        vk::ShaderModule computeShaderModule = createShaderModule(computeShaderCode);

        vk::PipelineShaderStageCreateInfo shaderStageCreateInfo {
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = computeShaderModule,
            .pName = "main"
        };

        vk::PipelineLayoutCreateInfo layoutCreateInfo {
            .setLayoutCount = 1,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
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

    void createCommandBuffer() {
        commandBuffers.resize(1);
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };
        if (logicalDevice.allocateCommandBuffers(&allocInfo, commandBuffers.data()) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to create command buffer!");
        }
    }

    void execute() {
        std::cout << "input data:\n";
        for (size_t i = 0; i < inputData.size(); ++i) {
            if (i % 64 == 0 && i != 0) std::cout << '\n';
            std::cout << inputData[i];
        }
        std::cout << "\n";
        
        vk::CommandBufferBeginInfo beginInfo {};
        if (commandBuffers[0].begin(&beginInfo) != vk::Result::eSuccess) {
            minilog::log_fatal("command buffer failed to begin!");
        }
        commandBuffers[0].bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
        commandBuffers[0].bindDescriptorSets(
            vk::PipelineBindPoint::eCompute,
            pipelineLayout,
            0,
            1,
            descriptorSets.data(),
            0,
            nullptr
        );
        commandBuffers[0].dispatch(4, 1, 1);
        commandBuffers[0].end();

        vk::SubmitInfo submitInfo {
            .waitSemaphoreCount = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[0],
            .signalSemaphoreCount = 0
        };

        if (computeQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            minilog::log_fatal("failed to submit command buffer!");
        }
        computeQueue.waitIdle();// wait the calculation to finish

        void* data = logicalDevice.mapMemory(storageBufferMemory, 0, sizeof(inputData), {});
        memcpy(outputData.data(), data, sizeof(inputData));
        logicalDevice.unmapMemory(storageBufferMemory);

        std::cout << "output data:\n";
        for (size_t i = 0; i < outputData.size(); ++i) {
            if (i % 64 == 0 && i != 0) std::cout << '\n';
            std::cout << outputData[i];
        }
        std::cout << '\n';
    }
};


int main() {
    HelloComputeShader app {};
    try {
        app.initVulkan();
    } catch (std::runtime_error e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
