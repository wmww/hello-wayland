project('minimal-wayland-apps',
    ['c'],
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=0.45.1',
    default_options: ['c_std=gnu11', 'warning_level=3'])

add_project_arguments(
    ['-Wno-pedantic', '-Wno-unused-parameter'],
    language: 'c')

wayland_client = dependency('wayland-client', version: '>=1.10.0')
# wayland_scanner is required, but we can find it without pkg-config
wayland_scanner = dependency('wayland-scanner', version: '>=1.10.0', required: false)
# use system xdg-shell protocol when available
wayland_protocols = dependency('wayland-protocols', version: '>=1.16', required: false)

subdir('protocol')

deps = [
    dependency('wayland-client'),
    dependency('wayland-egl'),
    dependency('egl'),
    dependency('gl')]

example = executable('layer-shell-subsurface',
    'layer-shell-subsurface.c',
    protocol_srcs,
    dependencies: deps)

example = executable('egl-window',
    'egl-window.c',
    protocol_srcs,
    dependencies: deps)
