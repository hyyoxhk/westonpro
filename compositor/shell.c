
#include <stddef.h>
#include <stdint.h>

#include <wlrston.h>

struct desktop_api {
	size_t struct_size;
	void (*move)(struct wet_view *view,
		     struct weston_seat *seat, uint32_t serial, void *user_data);
	void (*resize)(struct wet_view *view,
		       struct weston_seat *seat, uint32_t serial,
		       enum weston_desktop_surface_edge edges, void *user_data);
};

static void
desktop_surface_move(struct wet_view *view,
		     struct weston_seat *seat, uint32_t serial, void *shell)
{
        
}

static const struct desktop_api shell_desktop_api = {
	.struct_size = sizeof(struct desktop_api),
        .move = desktop_surface_move,

};

int
wet_shell_init(struct server *server, int *argc, char *argv[])
{

}
