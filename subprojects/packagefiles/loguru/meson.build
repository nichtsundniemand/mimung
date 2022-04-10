project('loguru')

dl_dep = meson.get_compiler('cpp').find_library('dl')
thread_dep = dependency('threads')

loguru_include = include_directories('.')
loguru_lib = static_library(
	'loguru',
	'loguru.cpp',
	dependencies: [dl_dep, thread_dep]
)

loguru_dep = declare_dependency(
	link_with: loguru_lib,
	include_directories: loguru_include
)

meson.override_dependency('loguru', loguru_dep)
