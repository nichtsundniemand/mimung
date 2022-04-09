#ifndef CONNECTION_HPP
	#define CONNECTION_HPP

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <wayland-egl.h>
#include <xdg-shell.h>

namespace wayland {
	/// \brief State of the application
	/**
	 * An instance of this class is passed to the wayland callbacks and
	 * other functions that might need to read or manipulate the global
	 * application state.
	 */
	class connection {
		private:
			/// True when the app is running
			bool running;
			/// Wayland display handle
			struct wl_display* display;
			/// Wayland registry handle
			struct wl_registry* registry;

			/// Wayland compositor handle
			struct wl_compositor* compositor;
			/// XDG shell WM handle
			struct xdg_wm_base* wm_base;

			/// Wayland window surface handle
			struct wl_surface* surface;
			/// XDG window surface handle
			struct xdg_surface* xdg_surface;
			/// XDG window toplevel handle
			struct xdg_toplevel* toplevel;
			/// Wayland EGL window handle
			struct wl_egl_window* egl_window;

			/// True if a new window layout should be applied
			bool have_configure;
			/// \{ New dimensions of XDG window
			int32_t conf_w, conf_h;
			/// \}

			/// EGL display handle
			EGLDisplay egl_display;
			/// EGL context handle
			EGLContext egl_context;
			/// EGL surface handle
			EGLSurface egl_surface;

			static void wl_callback_done_listener(
				void* wl_data, struct wl_callback* wl_callback, uint32_t callback_data
			);

			static constexpr struct wl_callback_listener wl_callback_listener = {
				wl_callback_done_listener
			};

			static void xdg_wm_base_ping_handler(
				void* wl_data, struct xdg_wm_base* wm_base, uint32_t serial
			);

			static constexpr struct xdg_wm_base_listener xdg_wm_base_listener = {
				xdg_wm_base_ping_handler
			};

			static void xdg_surface_configure_handler(
				void* wl_data, struct xdg_surface* xdg_surface, uint32_t serial
			);

			static constexpr struct xdg_surface_listener xdg_surface_listener = {
				xdg_surface_configure_handler
			};

			static void xdg_toplevel_configure_handler(
				void* wl_data, struct xdg_toplevel* xdg_toplevel,
				int32_t width, int32_t height, struct wl_array* states
			);

			static void xdg_toplevel_close_handler(
				void* wl_data, struct xdg_toplevel* xdg_toplevel
			);

			static constexpr struct xdg_toplevel_listener xdg_toplevel_listener = {
				xdg_toplevel_configure_handler,
				xdg_toplevel_close_handler
			};

			static void global_registry_handler(
				void* wl_data, struct wl_registry* registry, uint32_t id,
				const char* interface, uint32_t version
			);

			static constexpr struct wl_registry_listener registry_listener = {
				global_registry_handler,
				NULL
			};

		protected:
			struct wl_display *get_display();
			struct wl_surface *get_surface();

			virtual void on_resize(int32_t width, int32_t height) = 0;
			virtual void on_frame() = 0;
			virtual void on_close() = 0;

		public:
			connection();
			virtual ~connection() = default;

			void create_egl_surface();

			bool is_running();
			uint32_t get_width() const;
			uint32_t get_height() const;
			void dispatch();
	};
};

#endif
