add_subdirectory(7zip)

option(CLI11_PRECOMPILED "" ON)
add_subdirectory(CLI11)

add_subdirectory(fltk2)
add_subdirectory(lib3ds)

option(MZ_COMPAT "" OFF)
option(MZ_DECOMPRESS_ONLY "" ON)
add_subdirectory(minizip-ng)

add_subdirectory(spdlog)
add_subdirectory(simdutf)

# add_subdirectory(DevIL/DevIL)

add_subdirectory(TXPK_mini)

add_subdirectory(yaml-cpp)