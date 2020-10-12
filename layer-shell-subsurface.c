#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>
#include "wlr-layer-shell-unstable-v1-client.h"

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_subcompositor *subcompositor = NULL;
static struct zwlr_layer_shell_v1 *layer_shell = NULL;
static EGLDisplay egl_display;
static EGLContext egl_context;
static EGLConfig egl_config;
static char running = 1;

struct window {
    struct {
        struct wl_surface *surface;
        struct zwlr_layer_surface_v1 *layer_surface;
        struct wl_egl_window *egl_window;
        EGLSurface egl_surface;
    } main;
    struct {
        struct wl_surface *surface;
        struct wl_subsurface *subsurface;
        struct wl_egl_window *egl_window;
        EGLSurface egl_surface;
    } subsurface;
};

static void draw_surface (EGLSurface surface, float r, float g, float b) {
    eglMakeCurrent (egl_display, surface, surface, egl_context);
    glClearColor (r, g, b, 1.0);
    glClear (GL_COLOR_BUFFER_BIT);
    eglSwapBuffers (egl_display, surface);
}

// listeners
static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
    }
    if (!strcmp(interface, wl_subcompositor_interface.name)) {
        subcompositor = wl_registry_bind (registry, name, &wl_subcompositor_interface, 1);
    }
    else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name)) {
        layer_shell = wl_registry_bind (registry, name, &zwlr_layer_shell_v1_interface, 1);
    }
}
static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {}
static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

void layer_surface_configure (void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t serial, uint32_t width, uint32_t height) {
    struct window *window = data;
    zwlr_layer_surface_v1_ack_configure (window->main.layer_surface, serial);
    wl_egl_window_resize (window->main.egl_window, width, height, 0, 0);
    draw_surface (window->main.egl_surface, 0.0, 0.5, 1.0);
}
void layer_surface_closed (void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1) {}

static struct zwlr_layer_surface_v1_listener layer_surface_listener = {&layer_surface_configure, &layer_surface_closed};

static void create_window (struct window *window, int32_t width, int32_t height) {
    window->main.surface = wl_compositor_create_surface (compositor);
    window->main.layer_surface = zwlr_layer_shell_v1_get_layer_surface (
        layer_shell,
        window->main.surface,
        NULL,
        ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        "example");
    zwlr_layer_surface_v1_set_size (window->main.layer_surface, width, height);
    zwlr_layer_surface_v1_add_listener (window->main.layer_surface, &layer_surface_listener, window);
    window->main.egl_window = wl_egl_window_create (window->main.surface, width, height);
    window->main.egl_surface = eglCreateWindowSurface (egl_display, egl_config, window->main.egl_window, NULL);
    wl_surface_commit (window->main.surface);

    window->subsurface.surface = wl_compositor_create_surface (compositor);
    window->subsurface.subsurface = wl_subcompositor_get_subsurface (
        subcompositor,
        window->subsurface.surface,
        window->main.surface);
    wl_subsurface_set_desync (window->subsurface.subsurface);
    wl_subsurface_set_position (window->subsurface.subsurface, 100, 100);
    window->subsurface.egl_window = wl_egl_window_create (window->subsurface.surface, width - 40, height - 40);
    window->subsurface.egl_surface = eglCreateWindowSurface (egl_display, egl_config, window->subsurface.egl_window, NULL);
    wl_surface_commit (window->subsurface.surface);
    wl_display_roundtrip(display);
    draw_surface (window->subsurface.egl_surface, 0.0, 1.0, 0.5);
    wl_display_roundtrip(display);
}
static void delete_window (struct window *window) {
    eglDestroySurface (egl_display, window->main.egl_surface);
    wl_egl_window_destroy (window->main.egl_window);
    zwlr_layer_surface_v1_destroy (window->main.layer_surface);
    wl_surface_destroy (window->main.surface);
}

int main () {
    display = wl_display_connect (NULL);
    struct wl_registry *registry = wl_display_get_registry (display);
    wl_registry_add_listener (registry, &registry_listener, NULL);
    wl_display_roundtrip (display);

    egl_display = eglGetDisplay (display);
    eglInitialize (egl_display, NULL, NULL);

    eglBindAPI (EGL_OPENGL_API);
    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
    EGL_NONE};
    EGLint num_config;
    eglChooseConfig (egl_display, attributes, &egl_config, 1, &num_config);
    egl_context = eglCreateContext (egl_display, egl_config, EGL_NO_CONTEXT, NULL);

    struct window window;
    create_window (&window, 300, 300);

    while (running) {
        wl_display_dispatch_pending (display);
    }

    delete_window (&window);
    eglDestroyContext (egl_display, egl_context);
    eglTerminate (egl_display);
    wl_display_disconnect (display);
    return 0;
}
