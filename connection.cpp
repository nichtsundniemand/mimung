#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include <wayland-egl.h>

#include <loguru.hpp>

EGLint const attributes[] = {
	EGL_RED_SIZE, 1,
	EGL_GREEN_SIZE, 1,
	EGL_BLUE_SIZE, 1,
	EGL_NONE
};

EGLAttrib const debug_attr[] = {
	EGL_DEBUG_MSG_INFO_KHR, EGL_TRUE,
	EGL_DEBUG_MSG_WARN_KHR, EGL_TRUE,
	EGL_DEBUG_MSG_ERROR_KHR, EGL_TRUE,
	EGL_DEBUG_MSG_CRITICAL_KHR, EGL_TRUE,
	EGL_NONE
};

	/* void gl_redraw(GLuint arr_obj) {
		printf(__FILE__":%d(%s): GL section start!\n", __LINE__, __func__);
		// GLint cur_prog_id;
		// glGetIntegerv(GL_CURRENT_PROGRAM, &cur_prog_id);
		// char cur_prog_msg[snprintf(NULL, 0, gl_logf_cur_prog, cur_prog_id) + 1];
		// sprintf(cur_prog_msg, gl_logf_cur_prog, cur_prog_id);
		// glDebugMessageInsert(
		// 	GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER,
		// 	0, GL_DEBUG_SEVERITY_NOTIFICATION,
		// 	-1, cur_prog_msg
		// );

		glClearColor(1.0, 1.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(arr_obj);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glFlush();
		printf(__FILE__":%d(%s): GL section end\n", __LINE__, __func__);
	}*/

#include "connection.hpp"

namespace wayland {
	void connection::wl_callback_done_listener(
		void* wl_data, struct wl_callback* wl_callback, uint32_t callback_data
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called (%d ms)!", callback_data);

		static uint32_t starttime = 0;

		wl_callback_destroy(wl_callback);
		wl_callback = wl_surface_frame(((connection*)wl_data)->surface);
		wl_callback_add_listener(wl_callback, &wl_callback_listener, wl_data);

		if(starttime == 0 || callback_data < starttime)
	    		starttime = callback_data;
		// eglSwapBuffers(
		// 	((connection*)wl_data)->egl_display,
		// 	((connection*)wl_data)->egl_surface
		// );
	}

	void connection::xdg_wm_base_ping_handler(
		void* wl_data, struct xdg_wm_base* wm_base, uint32_t serial
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called!");

		xdg_wm_base_pong(wm_base, serial);

		LOG_F(INFO, " - pong(%d)", serial);
	}

	void connection::xdg_surface_configure_handler(
		void* wl_data, struct xdg_surface* xdg_surface, uint32_t serial
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called!");

		if(((connection*)wl_data)->have_configure) {
			connection *this_conn = (connection *)wl_data;
			struct vulkan_state& vulkan_state = this_conn->vulkan_state;

			if(vulkan_state.physical_device != VK_NULL_HANDLE && vulkan_state.surface != VK_NULL_HANDLE) {
				VkSurfaceCapabilitiesKHR surface_capabilities;
				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
					vulkan_state.physical_device, vulkan_state.surface, &surface_capabilities
				);

				vkDestroySwapchainKHR(vulkan_state.logical_device, vulkan_state.swapchain, nullptr);

				VkExtent2D surface_extent {
					.width = (uint32_t)this_conn->conf_w, // (uint32_t)((connection*)wl_data)->conf_w,
					.height = (uint32_t)this_conn->conf_h // (uint32_t)((connection*)wl_data)->conf_h,
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

			xdg_surface_ack_configure(xdg_surface, serial);
			LOG_F(INFO, " - ack_configure(%d)", serial);
			if(((connection*)wl_data)->egl_window != NULL) {
				wl_egl_window_resize(
					((connection*)wl_data)->egl_window,
					((connection*)wl_data)->conf_w,
					((connection*)wl_data)->conf_h,
					0, 0
				);
			}
			((connection*)wl_data)->have_configure = false;
			if(
				((connection*)wl_data)->conf_w > 0 &&
				((connection*)wl_data)->conf_h > 0
			) {
				// gl_redraw(
				// 	((connection*)wl_data)->vao.arr_obj
				// );
				eglSwapBuffers(
					((connection*)wl_data)->egl_display,
					((connection*)wl_data)->egl_surface
				);
			}
		}
	}

	void connection::xdg_toplevel_configure_handler(
		void* wl_data, struct xdg_toplevel* xdg_toplevel,
		int32_t width, int32_t height, struct wl_array* states
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called!");
		LOG_F(INFO, " - xdg_toplevel_configure_handler:");
		LOG_F(INFO, "   - width: %d", width);
		LOG_F(INFO, "   - height: %d", height);

		((connection*)wl_data)->have_configure = true;
		((connection*)wl_data)->conf_w = width;
		((connection*)wl_data)->conf_h = height;
	}

	void connection::xdg_toplevel_close_handler(
		void* wl_data, struct xdg_toplevel* xdg_toplevel
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called!");
		((connection*)wl_data)->running = false;
	}

	void connection::global_registry_handler(
	    void* wl_data, struct wl_registry* registry, uint32_t id,
	    const char* interface, uint32_t version
	) {
		LOG_SCOPE_FUNCTION(INFO);
		LOG_F(INFO, "Handler called!");
		if(strcmp(interface, "wl_compositor") == 0) {
			((connection*)wl_data)->compositor = 
				(struct wl_compositor *)wl_registry_bind(registry, id, &wl_compositor_interface, 1);
		} else if(strcmp(interface, "xdg_wm_base") == 0) {
			((connection*)wl_data)->wm_base = 
				(struct xdg_wm_base *)wl_registry_bind(registry, id, &xdg_wm_base_interface, 1);

			xdg_wm_base_add_listener(
				((connection*)wl_data)->wm_base,
				&xdg_wm_base_listener, wl_data
			);
		}
		LOG_F(INFO, " - Listener ran (%s)!", interface);
	}

	connection::connection() {
		LOG_SCOPE_FUNCTION(INFO);

		running = true;

		have_configure = false;
		conf_w = 0;
		conf_h = 0;

		display = wl_display_connect(NULL);

		registry = wl_display_get_registry(display);
		wl_registry_add_listener(registry, &registry_listener, this);

		wl_display_roundtrip(display);

		surface = wl_compositor_create_surface(compositor);

		xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
		xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, this);

		toplevel = xdg_surface_get_toplevel(xdg_surface);
		xdg_toplevel_add_listener(toplevel, &xdg_toplevel_listener, this);

		wl_surface_commit(surface);
		wl_display_roundtrip(display);

		LOG_F(INFO, "Wayland window setup complete!");

		struct wl_callback* frame_cb =
			wl_surface_frame(surface);
		wl_callback_add_listener(frame_cb, &wl_callback_listener, this);

		// return 0;
	}

	void connection::create_egl_surface() {
		LOG_SCOPE_FUNCTION(INFO);

		egl_display = 
			eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, display, NULL);
		if(egl_display == EGL_NO_DISPLAY) {
			LOG_F(ERROR, "Could not get EGL display!");
			// return 1;
		}
		LOG_F(INFO, "display = %p", egl_display);

		EGLint major = 0, minor = 0;
		if(eglInitialize(egl_display, &major, &minor) == EGL_FALSE) {
			LOG_F(ERROR, "Could not initialize EGL:");
			EGLint error = eglGetError();

			switch(error) {
				case EGL_BAD_DISPLAY:
	            			LOG_F(ERROR, "    - EGL_BAD_DISPLAY");
	    				break;
	    			case EGL_NOT_INITIALIZED:
	            			LOG_F(ERROR, "    - EGL_NOT_INITIALIZED");
	        			break;
	        		default:
	            			LOG_F(ERROR, "    - Unknown error");
			}

			// return 1;
		}
		LOG_F(INFO, "egl = (%d, %d)", major, minor);

		// if(egl_init_extensions() != 0) {
		// 	fprintf(stderr, "Error: Could not initialize EGL extensions!\n");
		// 	// return 1;
		// }

		// if(eglDebugMessageControlKHR(egl_debug_handler, debug_attr) != EGL_SUCCESS) {
		// 	fprintf(stderr, "Error: Could not setup EGL debugging!\n");
		// 	// return 1;
		// }
		// EGLAttrib check_val;
		// fprintf(stderr, "Setup EGL debugging:\n");
		// if(eglQueryDebugKHR(EGL_DEBUG_CALLBACK_KHR, &check_val) == EGL_TRUE)
		// 	fprintf(stderr, "  - EGL_DEBUG_CALLBACK_KHR: 0x%lx\n", check_val);
		// if(eglQueryDebugKHR(EGL_DEBUG_MSG_INFO_KHR, &check_val) == EGL_TRUE)
		// 	fprintf(stderr, "  - EGL_DEBUG_MSG_INFO_KHR: 0x%lx\n", check_val);
		// if(eglQueryDebugKHR(EGL_DEBUG_MSG_WARN_KHR, &check_val) == EGL_TRUE)
		// 	fprintf(stderr, "  - EGL_DEBUG_MSG_WARN_KHR: 0x%lx\n", check_val);
		// if(eglQueryDebugKHR(EGL_DEBUG_MSG_ERROR_KHR, &check_val) == EGL_TRUE)
		// 	fprintf(stderr, "  - EGL_DEBUG_MSG_ERROR_KHR: 0x%lx\n", check_val);
		// if(eglQueryDebugKHR(EGL_DEBUG_MSG_CRITICAL_KHR, &check_val) == EGL_TRUE)
		// 	fprintf(stderr, "  - EGL_DEBUG_MSG_CRITICAL_KHR: 0x%lx\n", check_val);

		EGLConfig config;
		EGLint num_config = 0;
		eglChooseConfig(egl_display, attributes, &config, 1, &num_config);
		LOG_F(INFO, "config = %p, num_config = %d", config, num_config);

		eglBindAPI(EGL_OPENGL_API);
		LOG_F(INFO, "Current EGL API:");
		switch(eglQueryAPI()) {
			case EGL_OPENGL_API:
				LOG_F(INFO, " - EGL_OPENGL_API");
				break;
			case EGL_OPENGL_ES_API:
				LOG_F(INFO, " - EGL_OPENGL_ES_API");
				break;
			case EGL_OPENVG_API:
				LOG_F(INFO, " - EGL_OPENVG_API");
				break;
			case EGL_NONE:
				LOG_F(INFO, " - EGL_NONE");
				break;
			default:
				LOG_F(INFO, " - Unknown");
		}
		EGLint const context_attr[] = {
			EGL_CONTEXT_MAJOR_VERSION, 4,
			EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
			EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
			EGL_NONE
		};

		if((egl_context =
			eglCreateContext(
				egl_display, config, EGL_NO_CONTEXT, context_attr
			)) == EGL_NO_CONTEXT
		) {
			LOG_F(ERROR, "Could not create EGL context!");
			// return 1;
		}
		LOG_F(INFO, "Created EGL context (%p)!", egl_context);

		egl_window = wl_egl_window_create(surface, 100, 100);
		egl_surface =
			eglCreatePlatformWindowSurface(egl_display, config, egl_window, NULL);
		LOG_F(INFO, "Created EGL surface (%p)!", egl_surface);

		eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

		eglSwapBuffers(egl_display, egl_surface);
	}

	void connection::create_vulkan_surface() {
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
			.display = display,
			.surface = surface,
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

	bool connection::is_running() {
		return running;
	}

	void connection::dispatch() {
		wl_display_dispatch(display);
	}
};
