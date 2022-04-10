project(
	'mimung', ['c', 'cpp'],
	version : '0.1',
	default_options : [
		'cpp_std=c++20',
		'warning_level=3',
	],
)

public_include = include_directories('include')
mimung_include = include_directories('include/mimung')

loguru_dep = dependency('loguru')
wayland_dep = dependency('wayland-client')
wayland_protocols_dep = dependency('wayland-protocols')

# Inspired by mesa's wayland-protocol generation
wayland_scanner_dep = dependency('wayland-scanner')
wayland_scanner_prog = find_program(wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'))

wayland_xdg_shell_xml = join_paths(
	wayland_protocols_dep.get_pkgconfig_variable('pkgdatadir'),
	'stable', 'xdg-shell', 'xdg-shell.xml'
)

wayland_xdg_shell_c = custom_target(
	'xdg-shell.c',
	input: wayland_xdg_shell_xml,
	output: 'xdg-shell.c',
	command: [wayland_scanner_prog, 'private-code', '@INPUT@', '@OUTPUT@']
)

wayland_xdg_shell_h = custom_target(
	'xdg-shell.h',
	input: wayland_xdg_shell_xml,
	output: 'xdg-shell.h',
	command: [wayland_scanner_prog, 'client-header', '@INPUT@', '@OUTPUT@']
)

wayland_xdg_shell_dep = declare_dependency(
	sources: [wayland_xdg_shell_c, wayland_xdg_shell_h],
	include_directories: '.',
)

mimung_lib = library(
	'mimung',
	'connection.cpp',
	dependencies: [
		loguru_dep,
		wayland_dep,
		wayland_xdg_shell_dep,
	],
	include_directories: [
		mimung_include,
		public_include,
	]
)

egl_dep = dependency('egl')
wayland_egl_dep = dependency('wayland-egl')

mimung_egl_lib = library(
	'mimung_egl',
	'connection_egl.cpp',
	dependencies: [
		egl_dep,
		loguru_dep,
		wayland_egl_dep,
	],
	link_with: mimung_lib,
	include_directories: [
		mimung_include
	]
)

mimung_dep = declare_dependency(
	link_with: mimung_lib,
	include_directories: public_include,
)

mimung_egl_dep = declare_dependency(
	link_with: mimung_egl_lib,
	include_directories: public_include,
)

meson.override_dependency(
	'mimung', mimung_dep
)

meson.override_dependency(
	'mimung-egl', mimung_egl_dep
)
