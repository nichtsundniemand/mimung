#ifndef CONNECTION_EGL_HPP
	#define CONNECTION_EGL_HPP

#include "connection.hpp"

#include <wayland-egl.h>

#include <EGL/egl.h>

namespace wayland {
	class connection_egl: public connection {
		public:
			struct egl_state {
				/// Wayland EGL window handle
				struct wl_egl_window* egl_window;
				/// EGL display handle
				EGLDisplay egl_display;
				/// EGL context handle
				EGLContext egl_context;
				/// EGL surface handle
				EGLSurface egl_surface;
			};

		private:
			struct egl_state egl_state;

			void on_resize(int32_t width, int32_t height) override;
			void on_frame() override;
			void on_close() override;

		public:
			connection_egl();
			~connection_egl() override = default;

			const struct egl_state *get_egl_state() const;
	};
};

#endif
