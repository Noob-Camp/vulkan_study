#include <vulkan/vulkan.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <stb/stb_image_write.h>
#include <cornell_box.h>

#include <cstdint>
#include <vector>
#include <cstring> // for strcmp
#include <string_view>


const std::vector<const char*> validation_layers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> instance_extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
const std::vector<const char*> logical_device_extensions = { VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME };


class PathTracingCornellBox {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;

	VkPhysicalDevice physical_device;
	std::optional<std::uint32_t> queue_family_index;
	VkDevice logical_device;
	VkQueue queue;

    VkBuffer uniform_buffer;
	VkDeviceMemory uniform_buffer_memory;
	std::array<VkBuffer, 5uz> storage_buffers;
	std::array <VkDeviceMemory, 5uz> storage_buffer_memorys;
    std::vector<float> vertices;

    void run() {
        if (!check_validation_layer_support()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        create_instance();
        pick_physcial_device();
        create_logical_device();

        create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniform_buffer, uniform_buffer_memory);
        for (std::size_t i = 0uz; i < storage_buffers.size(); ++i) {
            create_buffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, storage_buffers[i], storage_buffer_memorys[i]);
        }
    }

    void create_instance() {
        VkApplicationInfo app_info {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Hello Compute Shader",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0
        };

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = populate_debug_messenger_ci();
        VkInstanceCreateInfo instance_ci {
            .sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = &debugCreateInfo,
            .flags = 0u,
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<std::uint32_t>(validation_layers.size()),
            .ppEnabledLayerNames = validation_layers.data(),
            .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
            .ppEnabledExtensionNames = extensions.data();
        };

        auto extension_properties = query_instance_extension_properties();
        std::cout << extension_properties << std::endl;

        if (
            VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
            result != VkResult::VK_SUCCESS
        ) {
            throw std::runtime_error("failed to create instance");
        }

        if (
            VkResult result = createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debug_messenger);
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to setup debug messenger");
        }
    }

    void pick_physcial_device() {
        auto physical_devices = query_physical_devices(instance);
        std::cout << physical_devices << std::endl;
        for (const auto device : physical_devices) {
            auto queue_families_properties = query_physical_device_queue_family_properties(device);
            std::cout << queue_families_properties << std::endl;
            for (std::size_t i = 0; i < queue_families_properties.size(); ++i) {
                if (queue_families_properties[i].queueFlags & (VK_QUEUE_COMPUTE_BIT)) {
                    queue_family_index = i;
                    physical_device = device;
                    break;
                }
            }
            if (queue_family_index.has_value()) { break; }
        }
        if (!queue_family_index.has_value()) {
            throw std::runtime_error("can't find a family that contains compute&transfer queue!");
        } else {
            std::cout << "Select Physical Device:" << physical_device << std::endl;
            std::cout << "Select Queue Index:" << queueFamilyIndex.value() << std::endl;
        }
        auto properties = query_physical_device_properties(physical_device);
        std::cout << "maxComputeWorkGroupInvocations:" << properties.limits.maxComputeWorkGroupInvocations << std::endl;
        computeShaderProcessUnit = sqrt(properties.limits.maxComputeWorkGroupInvocations);
	}

    void create_logical_device() {
        auto priority { 1.0f }; // default
        VkDeviceQueueCreateInfo queueCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .queueFamilyIndex = queueFamilyIndex.value(),
            .queueCount = 1u,
            .pQueuePriorities = &priority
        };

        VkDeviceCreateInfo logical_device_ci {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .queueCreateInfoCount = 1u,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<std::uint32_t>(logical_device_extensions.size()),
            .ppEnabledExtensionNames = logical_device_extensions.data(),
            .pEnabledFeatures = nullptr
        };

        if (
            VkResult result = vkCreateDevice(physical_device, &logical_device_ci, nullptr, &logical_device);
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to create logical device");
        }
        vkGetDeviceQueue(logical_device, queueFamilyIndex.value(), 0u, &queue);
    }

    void create_buffer(VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo buffer_ci {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0u,
            .size = 1920u * 1080u * 4u,
            .usage = usage,
            .shardingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1u,
            .pQueueFamilyIndices = &queueFamilyIndex.value()
        };
        if (
            VkResult result = vkCreateBuffer(logical_device, &buffer_ci, nullptr, &buffer);
            result != VK_SUCCESS
        ) {
            throw std::runtime_error("failed to create buffer!");
        }

		VkMemoryRequirements requirements = query_memory_requirements(logical_device, buffer);
		std::cout << requirements << std::endl;

		VkMemoryAllocateInfo memory_allocate_info {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = requirements.size,
            .memoryTypeIndex = find_memory_type(
                physcial_device,
                requirements,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            )
        };

		if (
            VkResult result = vkAllocateMemory(logical_device, &memory_allocate_info, nullptr, &memory);
            result != VK_SUCCESS
        ) {
			throw std::runtime_error("failed to allocate buffer memory");
        }

		vkBindBufferMemory(logical_device, buffer, memory, 0u);
	}

    void prepare_scene_data() {
        tinyobj::ObjReaderConfig obj_reader_config;
        obj_reader_config.triangulate = true;
        obj_reader_config.triangulation_method = "simple";
        obj_reader_config.vertex_color = false;
        obj_reader_config.mtl_search_path = ""s;
        tinyobj::ObjReader obj_reader;
        if (!obj_reader.ParseFromString(cornell_box_string, "", obj_reader_config)) {
            std::string_view error_message = "unknown error.";
            if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
            std::cout << "Failed to load OBJ file: " << error_message << std::endl;
        }
        if (auto &&e = obj_reader.Warning(); !e.empty()) { std::cout << e << std::endl; }

        auto &&p = obj_reader.GetAttrib().vertices;
        vertices.reserve(p.size());
        for (std::uint32_t i = 0u; i < p.size(); ++i) { vertices.emplace_back(p[i]); }
        std::cout << "Loaded mesh with " << obj_reader.GetShapes().size() << " shapes"
                << " and " << vertices.size() << " vertices."
                << std::endl;
    }

    std::uint32_t find_memory_type(
        const VkMemoryRequirements& requirements,
        VkMemoryPropertyFlags properties
    ) {
        VkPhysicalDeviceMemoryProperties memory_properties = query_physical_device_memory_properties(physical_device);
        std::cout << memory_properties << std::endl;
        for (std::uint32_t i = 0u; i < memory_properties.memoryTypeCount; ++i) {
            if (
                requirements.memoryTypeBits & (1 << i)
                && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties
            ) {
                std::cout << "pick memory type [" << i << "]\n";
                return i;
            }
        }
    }

    void read_memory(VkDeviceMemory memory, void* dataBlock, VkDeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0, size, 0, &data) != VK_SUCCESS) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(dataBlock, data, size);
        vkUnmapMemory(device, memory);
    }

    void write_memory(VkDeviceMemory memory, void* dataBlock, VkDeviceSize size) {
        void* data { nullptr };
        if (vkMapMemory(logical_device, memory, 0u, size, 0u, &data) != VK_SUCCESS) {
            throw std::runtime_error("failed to map memory");
        }
        memcpy(data, dataBlock, size);
        vkUnmapMemory(device, memory);
    }
}

void check_validation_layer_support() {
    uint32_t layerCount { 0u };
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        for (const VkLayerProperties& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                validationLayersSupported = true;
                std::cout << "the " << layerName << " is supported!" << std::endl;
                break;
            }
        }
        if (validationLayersSupported) { break; }
    }

    if (enableValidationLayers && (!validationLayersSupported)) {
        std::cout << "validation layers requested, but not available!" << std::endl;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT populate_debug_messenger_ci() {
    VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_ci {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0u,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = nullptr
    };

    return debug_utils_messenger_ci;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        throw std::runtime_error("can't find proc: vkCreateDebugUtilsMessengerEXT");
    }
}

std::vector<VkExtensionProperties> query_instance_extension_properties() {
    std::uint32_t extension_count { 0u };
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extension_properties(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data());
    return extension_properties;
}

std::vector<VkPhysicalDevice> query_physical_devices(VkInstance instance) {
    std::uint32_t device_count { 0u };
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());
    return physical_devices;
}

std::vector<VkQueueFamilyProperties> query_physical_device_queue_family_properties(VkPhysicalDevice physical_device) {
    std::uint32_t property_count { 0u };
    vkGetPhysicalDeviceQueueFamilyProperties(device, &property_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_family_properties(property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &property_count, queue_family_properties.data());
    return queue_family_properties;
}

VkPhysicalDeviceProperties query_physical_device_properties(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(device, &physical_device_properties);
    return physical_device_properties;
}

VkMemoryRequirements query_memory_requirements(VkDevice device, VkBuffer buffer) {
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);
    return memory_requirements;
}

VkPhysicalDeviceMemoryProperties query_physical_device_memory_properties(VkPhysicalDevice physical_device) {
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
    return physical_device_memory_properties;
}


int main() {
    PathTracingCornellBox application();

    try {
        application.run();
    } catch (std::runtime_error e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
