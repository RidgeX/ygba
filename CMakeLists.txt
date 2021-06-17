cmake_minimum_required(VERSION 3.12)

option(USE_VCPKG "Use vcpkg" ON)

if(USE_VCPKG AND DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(ygba)

find_package(Freetype REQUIRED)
find_package(SDL2 REQUIRED)

add_subdirectory(lib/gl3w)
add_subdirectory(lib/imgui)
add_subdirectory(src)
