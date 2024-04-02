srcs_libshared = [
	'os-compatibility.c',
]

lib_libshared = static_library(
	'shared',
	srcs_libshared,
	include_directories: inc_public,
	pic: true,
	install: false
)

dep_libshared = declare_dependency(
	link_with: lib_libshared,
	include_directories: inc_public,
)