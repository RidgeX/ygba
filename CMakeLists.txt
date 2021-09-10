cmake_minimum_required(VERSION 3.15)

set(default_build_type "RelWithDebInfo")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(USE_VCPKG "Use vcpkg" ON)

if(USE_VCPKG AND DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "CMake toolchain file")
endif()

project(ygba LANGUAGES C CXX)

if(NOT USE_VCPKG)
    list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

foreach(lang C CXX)
    if(MSVC)
        string(APPEND CMAKE_${lang}_FLAGS " /W4")
        string(REPLACE " /DNDEBUG" "" CMAKE_${lang}_FLAGS_MINSIZEREL "${CMAKE_${lang}_FLAGS_MINSIZEREL}")
        string(REPLACE " /DNDEBUG" "" CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE}")
        string(REPLACE " /DNDEBUG" "" CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}")
    else()
        string(APPEND CMAKE_${lang}_FLAGS " -Wall -Wextra")
        string(REPLACE " -DNDEBUG" "" CMAKE_${lang}_FLAGS_MINSIZEREL "${CMAKE_${lang}_FLAGS_MINSIZEREL}")
        string(REPLACE " -DNDEBUG" "" CMAKE_${lang}_FLAGS_RELEASE "${CMAKE_${lang}_FLAGS_RELEASE}")
        string(REPLACE " -DNDEBUG" "" CMAKE_${lang}_FLAGS_RELWITHDEBINFO "${CMAKE_${lang}_FLAGS_RELWITHDEBINFO}")
    endif()
endforeach()

find_package(Freetype REQUIRED)
find_package(SDL2 REQUIRED)

add_subdirectory(lib/imgui)
add_subdirectory(src)
