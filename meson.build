project(
	'mimung', 'cpp',
	version : '0.1',
	default_options : [
		'cpp_std=c++20',
		'warning_level=3',
	],
)

public_include = include_directories('include')
mimung_include = include_directories('include/mimung')

egl_dep = dependency('egl')
loguru_dep = dependency('loguru')
wayland_dep = dependency('wayland-client')
wayland_egl_dep = dependency('wayland-egl')

mimung_lib = library(
	'mimung',
	[
		'connection.cpp',
		'xdg-shell.c',
	],
	dependencies: [
		egl_dep,
		loguru_dep,
		wayland_dep,
		wayland_egl_dep,
	],
	include_directories: [
		mimung_include,
		public_include,
	]
)

mimung_dep = declare_dependency(
	link_with: mimung_lib,
	include_directories: public_include,
)

meson.override_dependency(
	'mimung', mimung_dep
)
