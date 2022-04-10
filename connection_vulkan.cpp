#include "connection_vulkan.hpp"

#include <loguru.hpp>

#include <vulkan/vulkan_wayland.h>

#include <volcano/swapchain_surface.hpp>

namespace mimung {
	connection_vulkan::connection_vulkan() {
		LOG_SCOPE_FUNCTION(INFO);

		// Get an instance-handle
		std::vector<const char *> enabled_extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
		};

		VkInstanceCreateInfo create_info = {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.enabledExtensionCount = (uint32_t)enabled_extensions.size(),
			.ppEnabledExtensionNames = enabled_extensions.data()
		};

		vkCreateInstance(&create_info, nullptr, &vulkan_state.instance);

		// Select a physical device
		uint32_t physical_device_count;
		vkEnumeratePhysicalDevices(vulkan_state.instance, &physical_device_count, nullptr);

		LOG_F(INFO, "Physical devices available: %d", physical_device_count);

		std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
		vkEnumeratePhysicalDevices(vulkan_state.instance, &physical_device_count, physical_devices.data());

		{
			LOG_SCOPE_F(INFO, "Physical devices queried: %d", physical_device_count);
			for(auto& physical_device: physical_devices) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(physical_device, &properties);

				LOG_SCOPE_F(INFO, "deviceName: %s", properties.deviceName);
				LOG_F(
					INFO, "apiVersion: %d.%d.%d",
					VK_API_VERSION_MAJOR(properties.apiVersion),
					VK_API_VERSION_MINOR(properties.apiVersion),
					VK_API_VERSION_PATCH(properties.apiVersion)
				);
			}
		}

		vulkan_state.physical_device = physical_devices[0];
		VkPhysicalDeviceProperties physical_device_properties;
		vkGetPhysicalDeviceProperties(vulkan_state.physical_device, &physical_device_properties);
		LOG_F(INFO, "Selecting physical device 0: %s", physical_device_properties.deviceName);

		// Create logical device connection
		std::vector<const char *> enabled_device_extensions {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		std::vector<float> queue_priorities = {
			1,
		};

		std::vector<VkDeviceQueueCreateInfo> device_queue_infos {
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = 0,
				.queueCount = (uint32_t)queue_priorities.size(),
				.pQueuePriorities = queue_priorities.data(),
			}
		};

		VkDeviceCreateInfo device_create_info = {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = (uint32_t)device_queue_infos.size(),
			.pQueueCreateInfos = device_queue_infos.data(),
			.enabledExtensionCount = (uint32_t)enabled_device_extensions.size(),
			.ppEnabledExtensionNames = enabled_device_extensions.data(),
		};

		vkCreateDevice(vulkan_state.physical_device, &device_create_info, nullptr, &vulkan_state.logical_device);

		LOG_F(INFO, "device created! :)");

		// Create Wayland surface
		VkWaylandSurfaceCreateInfoKHR surface_create_info = {
			.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
			.display = get_display(),
			.surface = get_surface(),
		};

		vkCreateWaylandSurfaceKHR(
			vulkan_state.instance, &surface_create_info, nullptr, &vulkan_state.surface
		);

		// Create swapchain
		VkSurfaceCapabilitiesKHR surface_capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			vulkan_state.physical_device, vulkan_state.surface, &surface_capabilities
		);

		uint32_t surface_format_count;
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			vulkan_state.physical_device, vulkan_state.surface, &surface_format_count, nullptr
		);

		LOG_F(INFO, "Surface formats available: %d", surface_format_count);

		std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			vulkan_state.physical_device, vulkan_state.surface, &surface_format_count, surface_formats.data()
		);

		{
			LOG_SCOPE_F(INFO, "Surface formats queried: %d", surface_format_count);

			bool found = false;
			for(auto& surface_format: surface_formats) {
				if(surface_format.format == VK_FORMAT_R8G8B8A8_SRGB) {
					LOG_F(INFO, "Found format with VK_FORMAT_R8G8B8A8_SRGB!");
					vulkan_state.selected_surface_format = surface_format;
					found = true;
					break;
				}
			}

			if(!found) {
				LOG_F(ERROR, "Could not find an appropiate surface-format");
			}
		}

		uint32_t present_mode_count;
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			vulkan_state.physical_device, vulkan_state.surface, &present_mode_count, nullptr
		);

		LOG_F(INFO, "Present modes available: %d", present_mode_count);

		std::vector<VkPresentModeKHR> present_modes(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			vulkan_state.physical_device, vulkan_state.surface, &present_mode_count, present_modes.data()
		);

		{
			LOG_SCOPE_F(INFO, "Present modes queried: %d", present_mode_count);

			bool found = false;
			for(auto& present_mode: present_modes) {
				if(present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
					LOG_F(INFO, "Found present mode VK_PRESENT_MODE_MAILBOX_KHR!");
					vulkan_state.selected_present_mode = present_mode;
					found = true;
					break;
				}
			}

			if(!found) {
				LOG_F(ERROR, "Could not find preffered present mode - defaulting to first found");
				vulkan_state.selected_present_mode = present_modes[0];
			}
		}

		// At this point our wayland surface should be sizeless. Since we have to
		//  set something, we just use the minimal allowed surface extend reported
		//  by vulkan
		VkExtent2D surface_extent = surface_capabilities.minImageExtent;

		uint32_t queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(vulkan_state.physical_device, &queue_family_count, nullptr);

		VkSwapchainCreateInfoKHR swapchain_create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = vulkan_state.surface,
			.minImageCount = surface_capabilities.minImageCount,
			.imageFormat = vulkan_state.selected_surface_format.format,
			.imageColorSpace = vulkan_state.selected_surface_format.colorSpace,
			.imageExtent = surface_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = vulkan_state.selected_present_mode,
		};

		vkCreateSwapchainKHR(vulkan_state.logical_device, &swapchain_create_info, nullptr, &vulkan_state.swapchain);

		LOG_F(INFO, "Queue families available: %d", queue_family_count);

		std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(
			vulkan_state.physical_device, &queue_family_count, queue_family_properties.data()
		);

		{
			LOG_SCOPE_F(INFO, "Queue families queried: %d", queue_family_count);

			for(auto& queue_family: queue_family_properties) {
				LOG_F(INFO, "flags: %d", queue_family.queueFlags);
				LOG_F(INFO, "count: %d", queue_family.queueCount);
			}
		}

		vkGetDeviceQueue(vulkan_state.logical_device, 0, 0, &vulkan_state.queue);

		uint32_t image_index = 0;
		VkPresentInfoKHR present_info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.swapchainCount = 1,
			.pSwapchains = &vulkan_state.swapchain,
			.pImageIndices = &image_index,
		};
		vkQueuePresentKHR(vulkan_state.queue, &present_info);
	}

	const struct connection_vulkan::vulkan_state *connection_vulkan::get_vulkan_state() const {
		return &vulkan_state;
	}

	void connection_vulkan::on_resize(int32_t width, int32_t height) {
		VkSurfaceCapabilitiesKHR surface_capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			vulkan_state.physical_device, vulkan_state.surface, &surface_capabilities
		);

		vkDestroySwapchainKHR(vulkan_state.logical_device, vulkan_state.swapchain, nullptr);

		VkExtent2D surface_extent {
			.width = (uint32_t)width,
			.height = (uint32_t)height
		};

		VkSwapchainCreateInfoKHR swapchain_create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = vulkan_state.surface,
			.minImageCount = surface_capabilities.minImageCount,
			.imageFormat = vulkan_state.selected_surface_format.format,
			.imageColorSpace = vulkan_state.selected_surface_format.colorSpace,
			.imageExtent = surface_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = vulkan_state.selected_present_mode,
		};

		vkCreateSwapchainKHR(vulkan_state.logical_device, &swapchain_create_info, nullptr, &vulkan_state.swapchain);

		LOG_F(INFO, "Recreated swapchain");

		uint32_t image_index = 0;
		VkPresentInfoKHR present_info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.swapchainCount = 1,
			.pSwapchains = &vulkan_state.swapchain,
			.pImageIndices = &image_index,
		};
		vkQueuePresentKHR(vulkan_state.queue, &present_info);
	}

	void connection_vulkan::on_frame() {
		vulkan_state.swapchain->acquire_next_frame();
		vulkan_state.swapchain->present(vulkan_state.queue);
	}

	void connection_vulkan::on_close() {

	}
};
