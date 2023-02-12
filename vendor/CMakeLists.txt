add_subdirectory(7zip)

option(CLI11_PRECOMPILED "" ON)
add_subdirectory(fltk2)
add_subdirectory(lib3ds)

option(MZ_COMPAT "" OFF)
option(MZ_DECOMPRESS_ONLY "" ON)
add_subdirectory(minizip-ng)

add_subdirectory(spdlog)