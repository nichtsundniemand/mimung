#ifndef CONNECTION_VULKAN_HPP
	#define CONNECTION_VULKAN_HPP

#include "connection.hpp"

#include <vulkan/vulkan.h>

namespace wayland {
	class connection_vulkan: public connection {
		public:
			struct vulkan_state {
				VkInstance instance;
				VkPhysicalDevice physical_device;
				VkDevice logical_device;

				VkSurfaceKHR surface;
				VkSurfaceFormatKHR selected_surface_format;
				VkPresentModeKHR selected_present_mode;
				VkSwapchainKHR swapchain;
				VkQueue queue;
			};

		private:
			struct vulkan_state vulkan_state;

			void on_resize(int32_t width, int32_t height) override;
			void on_frame() override;
			void on_close() override;

		public:
			connection_vulkan();
			~connection_vulkan() override = default;

			const struct vulkan_state *get_vulkan_state() const;
	};
};

#endif
