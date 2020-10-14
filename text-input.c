#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xdg-shell-client.h"
#include "text-input-unstable-v3-client.h"

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_seat *seat = NULL;
static struct zwp_text_input_v3 *text_input = NULL;
static struct wl_pointer *pointer = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct zwp_text_input_manager_v3 *text_input_manager = NULL;
static EGLDisplay egl_display;
static EGLContext egl_context;
static EGLConfig egl_config;
static char quit = 0;

struct surface {
    struct wl_surface *surface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
};

struct window {
    struct surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    int width, height;
    char state;
    struct surface *cursor;
};

static void draw_window(struct window *window);

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static struct xdg_wm_base_listener xdg_wm_base_listener = {&xdg_wm_base_ping};

static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (!strcmp(interface, wl_seat_interface.name)) {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 4);
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (!strcmp(interface, zwp_text_input_manager_v3_interface.name)) {
        text_input_manager = wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) {}

static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    struct window *window = data;
    wl_pointer_set_cursor(wl_pointer, serial, window->cursor->surface, 10, 10);
}

static void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface) {}

static void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {}

static void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    struct window *window = data;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        window->state = !window->state;
        draw_window(window);
        if (window->state) {
            zwp_text_input_v3_enable(text_input);
            zwp_text_input_v3_set_surrounding_text(text_input, "", 0, 0);
            zwp_text_input_v3_set_text_change_cause(text_input, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_OTHER);
            zwp_text_input_v3_commit(text_input);
            zwp_text_input_v3_enable(text_input);
            zwp_text_input_v3_set_surrounding_text(text_input, "", 0, 0);
            zwp_text_input_v3_set_text_change_cause(text_input, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
            zwp_text_input_v3_set_content_type(text_input, 0, ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
            zwp_text_input_v3_set_cursor_rectangle(text_input, 10, 10, 0, 20);
            zwp_text_input_v3_commit(text_input);
        } else {
            zwp_text_input_v3_disable(text_input);
            zwp_text_input_v3_commit(text_input);
        }
    }
}

static void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {}

static struct wl_pointer_listener pointer_listener = {&pointer_enter, &pointer_leave, &pointer_motion, &pointer_button, &pointer_axis, NULL, NULL, NULL, NULL};

void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    struct window *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    wl_egl_window_resize(window->surface->egl_window, window->width, window->height, 0, 0);
    draw_window(window);
}
static struct xdg_surface_listener xdg_surface_listener = {&xdg_surface_configure};

void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    struct window *window = data;
    if (width > 0)
        window->width = width;
    if (height > 0)
        window->height = height;
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    quit = 1;
}

static struct xdg_toplevel_listener xdg_toplevel_listener = {&xdg_toplevel_configure, &xdg_toplevel_close};

static void text_input_enter(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, struct wl_surface *surface) {
    struct window *window = data;
    if (window->state) {
        zwp_text_input_v3_enable(zwp_text_input_v3);
        zwp_text_input_v3_set_surrounding_text(zwp_text_input_v3, "", 0, 0);
        zwp_text_input_v3_set_text_change_cause(zwp_text_input_v3, ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);
        zwp_text_input_v3_set_content_type(zwp_text_input_v3, 0, ZWP_TEXT_INPUT_V3_CONTENT_PURPOSE_NORMAL);
        zwp_text_input_v3_set_cursor_rectangle(zwp_text_input_v3, 10, 10, 0, 20);
        zwp_text_input_v3_commit(zwp_text_input_v3);
    }
}

static void text_input_leave(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, struct wl_surface *surface) {}

static void text_input_preedit_string(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, const char *text, int32_t cursor_begin, int32_t cursor_end) {}

static void text_input_commit_string(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, const char *text) {}

static void text_input_delete_surrounding_text(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t before_length, uint32_t after_length) {}

static void text_input_done(void *data, struct zwp_text_input_v3 *zwp_text_input_v3, uint32_t serial) {}

static struct zwp_text_input_v3_listener text_input_listener = {&text_input_enter, &text_input_leave, &text_input_preedit_string, &text_input_commit_string, &text_input_delete_surrounding_text, &text_input_done};

static struct surface *create_surface(int width, int height) {
    struct surface *surface = malloc(sizeof(struct surface));
    memset(surface, 0, sizeof(struct surface));
    surface->surface = wl_compositor_create_surface(compositor);
    surface->egl_window = wl_egl_window_create(surface->surface, width, height);
    surface->egl_surface = eglCreateWindowSurface(egl_display, egl_config, surface->egl_window, NULL);
    return surface;
}

static void draw_surface(struct surface *surface, float r, float g, float b) {
    eglMakeCurrent(egl_display, surface->egl_surface, surface->egl_surface, egl_context);
    glClearColor(r, g, b, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(egl_display, surface->egl_surface);
}

static void destroy_surface(struct surface *surface) {
    eglDestroySurface(egl_display, surface->egl_surface);
    wl_egl_window_destroy(surface->egl_window);
    wl_surface_destroy(surface->surface);
    free(surface);
}

static struct window *create_window(int width, int height) {
    struct window *window = malloc(sizeof(struct window));
    memset(window, 0, sizeof(struct window));

    window->surface = create_surface(width, height);

    window->xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, window->surface->surface);

    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
    window->width = width;
    window->height = height;

    wl_surface_commit(window->surface->surface);

    window->cursor = create_surface(30, 30);
    draw_surface(window->cursor, 1.0, 1.0, 1.0);

    return window;
}

static void draw_window(struct window *window) {
    if (window->state) {
        draw_surface(window->surface, 0.2, 0.6, 1.0);
    } else {
        draw_surface(window->surface, 0.0, 0.2, 0.4);
    }
}

static void destroy_window(struct window *window) {
    xdg_toplevel_destroy(window->xdg_toplevel);
    xdg_surface_destroy(window->xdg_surface);
    destroy_surface(window->surface);
    destroy_surface(window->cursor);
    free(window);
}

int main() {
    display = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    egl_display = eglGetDisplay(display);
    eglInitialize(egl_display, NULL, NULL);

    eglBindAPI(EGL_OPENGL_API);
    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
    EGL_NONE};
    EGLint num_config;
    eglChooseConfig(egl_display, attributes, &egl_config, 1, &num_config);
    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, NULL);

    struct window *window = create_window(300, 300);

    pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(pointer, &pointer_listener, window);

    text_input = zwp_text_input_manager_v3_get_text_input(text_input_manager, seat);
    zwp_text_input_v3_add_listener(text_input, &text_input_listener, window);

    while (wl_display_dispatch(display) != -1 && !quit) {}

    wl_pointer_destroy(pointer);
    destroy_window(window);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);
    wl_display_disconnect(display);
    return 0;
}
