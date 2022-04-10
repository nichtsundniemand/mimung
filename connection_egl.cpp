#include "connection_egl.hpp"

#include <loguru.hpp>

#include <EGL/eglext.h>

#include <stdio.h>
#include "egl_support.h"

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

namespace mimung {
	connection_egl::connection_egl() {
		LOG_SCOPE_FUNCTION(INFO);

		egl_state.egl_display = 
			eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, get_display(), NULL);
		if(egl_state.egl_display == EGL_NO_DISPLAY) {
			LOG_F(ERROR, "Could not get EGL display!");
			// return 1;
		}
		LOG_F(INFO, "display = %p", egl_state.egl_display);

		EGLint major = 0, minor = 0;
		if(eglInitialize(egl_state.egl_display, &major, &minor) == EGL_FALSE) {
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

		if(egl_init_extensions() != 0) {
			fprintf(stderr, "Error: Could not initialize EGL extensions!\n");
			// return 1;
		}

		if(eglDebugMessageControlKHR(egl_debug_handler, debug_attr) != EGL_SUCCESS) {
			fprintf(stderr, "Error: Could not setup EGL debugging!\n");
			// return 1;
		}
		EGLAttrib check_val;
		fprintf(stderr, "Setup EGL debugging:\n");
		if(eglQueryDebugKHR(EGL_DEBUG_CALLBACK_KHR, &check_val) == EGL_TRUE)
			fprintf(stderr, "  - EGL_DEBUG_CALLBACK_KHR: 0x%lx\n", check_val);
		if(eglQueryDebugKHR(EGL_DEBUG_MSG_INFO_KHR, &check_val) == EGL_TRUE)
			fprintf(stderr, "  - EGL_DEBUG_MSG_INFO_KHR: 0x%lx\n", check_val);
		if(eglQueryDebugKHR(EGL_DEBUG_MSG_WARN_KHR, &check_val) == EGL_TRUE)
			fprintf(stderr, "  - EGL_DEBUG_MSG_WARN_KHR: 0x%lx\n", check_val);
		if(eglQueryDebugKHR(EGL_DEBUG_MSG_ERROR_KHR, &check_val) == EGL_TRUE)
			fprintf(stderr, "  - EGL_DEBUG_MSG_ERROR_KHR: 0x%lx\n", check_val);
		if(eglQueryDebugKHR(EGL_DEBUG_MSG_CRITICAL_KHR, &check_val) == EGL_TRUE)
			fprintf(stderr, "  - EGL_DEBUG_MSG_CRITICAL_KHR: 0x%lx\n", check_val);

		EGLConfig config;
		EGLint num_config = 0;
		eglChooseConfig(egl_state.egl_display, attributes, &config, 1, &num_config);
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

		if((egl_state.egl_context =
			eglCreateContext(
				egl_state.egl_display, config, EGL_NO_CONTEXT, context_attr
			)) == EGL_NO_CONTEXT
		) {
			LOG_F(ERROR, "Could not create EGL context!");
			// return 1;
		}
		LOG_F(INFO, "Created EGL context (%p)!", egl_state.egl_context);

		egl_state.egl_window = wl_egl_window_create(get_surface(), 100, 100);
		egl_state.egl_surface =
			eglCreatePlatformWindowSurface(egl_state.egl_display, config, egl_state.egl_window, NULL);
		LOG_F(INFO, "Created EGL surface (%p)!", egl_state.egl_surface);

		eglMakeCurrent(egl_state.egl_display, egl_state.egl_surface, egl_state.egl_surface, egl_state.egl_context);

		eglSwapBuffers(egl_state.egl_display, egl_state.egl_surface);
	}

	const struct connection_egl::egl_state *connection_egl::get_egl_state() const {
		return &egl_state;
	}

	void connection_egl::on_resize(int32_t width, int32_t height) {
		wl_egl_window_resize(egl_state.egl_window, width, height, 0, 0);

		// gl_redraw(
		// 	((connection*)wl_data)->vao.arr_obj
		// );
		eglSwapBuffers(
			egl_state.egl_display,
			egl_state.egl_surface
		);
	}

	void connection_egl::on_frame() {
		eglSwapBuffers(
			egl_state.egl_display,
			egl_state.egl_surface
		);
	}

	void connection_egl::on_close() {

	}
};
