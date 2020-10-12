#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xdg-shell-client.h"

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static EGLDisplay egl_display;
static EGLContext egl_context;
static EGLConfig egl_config;
static char quit = 0;

struct window {
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    int width, height;
};

static void draw_surface(EGLSurface surface, float r, float g, float b) {
    eglMakeCurrent(egl_display, surface, surface, egl_context);
    glClearColor(r, g, b, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(egl_display, surface);
}

static void registry_add_object(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    }
    else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    }
}

static void registry_remove_object(void *data, struct wl_registry *registry, uint32_t name) {}

static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    struct window *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    if (!window->egl_window) {
        window->egl_window = wl_egl_window_create(window->surface, window->width, window->height);
        window->egl_surface = eglCreateWindowSurface(egl_display, egl_config, window->egl_window, NULL);
    } else {
        wl_egl_window_resize(window->egl_window, window->width, window->height, 0, 0);
    }
    draw_surface(window->egl_surface, 0.0, 0.5, 1.0);
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

static struct window *create_window(int width, int height) {
    struct window *window = malloc(sizeof(struct window));
    memset(window, 0, sizeof(struct window));
    window->surface = wl_compositor_create_surface(compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, window->surface);
    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
    wl_surface_commit(window->surface);
    window->egl_window = NULL;
    window->egl_surface = NULL;
    window->width = width;
    window->height = height;
    return window;
}

static void destroy_window(struct window *window) {
    if (window->egl_surface)
        eglDestroySurface(egl_display, window->egl_surface);
    if (window->egl_window)
        wl_egl_window_destroy(window->egl_window);
    xdg_toplevel_destroy(window->xdg_toplevel);
    xdg_surface_destroy(window->xdg_surface);
    wl_surface_destroy(window->surface);
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

    while (wl_display_dispatch(display) != -1 && !quit) {}

    destroy_window(window);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);
    wl_display_disconnect(display);
    return 0;
}
