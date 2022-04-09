#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <vulkan/vulkan.h>

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

		((connection *)wl_data)->on_frame();
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

				this_conn->on_resize(this_conn->conf_w, this_conn->conf_h);
			}

			xdg_surface_ack_configure(xdg_surface, serial);
			LOG_F(INFO, " - ack_configure(%d)", serial);
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
		((connection*)wl_data)->on_close();
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

	struct wl_display *connection::get_display() {
		return display;
	}

	struct wl_surface *connection::get_surface() {
		return surface;
	}

	connection::connection() {
		LOG_SCOPE_FUNCTION(INFO);

		egl_window = nullptr;

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
		// FIXME: Cannot roundtrip at this point because
		//   of virtual dispatch :/
		//   
		// wl_display_roundtrip(display);

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

	bool connection::is_running() {
		return running;
	}

	uint32_t connection::get_width() const {
		return conf_w;
	}

	uint32_t connection::get_height() const {
		return conf_h;
	}

	void connection::dispatch() {
		wl_display_dispatch(display);
	}
};
