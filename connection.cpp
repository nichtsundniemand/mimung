#include <fcntl.h>
#include <cstring>
#include <string>
#include <vector>

#include <loguru.hpp>

#include "connection.hpp"

namespace mimung {
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

		connection *this_conn = (connection *)wl_data;

		if(this_conn->have_configure) {
			this_conn->have_configure = false;
			if(this_conn->conf_w > 0 && this_conn->conf_h > 0)
				this_conn->on_resize(this_conn->conf_w, this_conn->conf_h);
			
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
