include(FindPackageHandleStandardArgs)

find_path(SDL2_INCLUDE_DIR NAMES SDL2/SDL.h)
find_library(SDL2_LIBRARY NAMES SDL2)
find_library(SDL2_MAIN_LIBRARY NAMES SDL2main)

if(SDL2_INCLUDE_DIR AND EXISTS "${SDL2_INCLUDE_DIR}/SDL2/SDL_version.h")
    file(STRINGS "${SDL2_INCLUDE_DIR}/SDL2/SDL_version.h" version_file
        REGEX "#define[ \t]+SDL_(MAJOR_VERSION|MINOR_VERSION|PATCHLEVEL).*")
    list(GET version_file 0 major_line)
    list(GET version_file 1 minor_line)
    list(GET version_file 2 patch_line)
    string(REGEX REPLACE "^#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MAJOR "${major_line}")
    string(REGEX REPLACE "^#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_MINOR "${minor_line}")
    string(REGEX REPLACE "^#define[ \t]+SDL_PATCHLEVEL[ \t]+([0-9]+)$" "\\1" SDL2_VERSION_PATCH "${patch_line}")
    set(SDL2_VERSION "${SDL2_VERSION_MAJOR}.${SDL2_VERSION_MINOR}.${SDL2_VERSION_PATCH}" CACHE STRING "SDL2 version")
endif()

if(SDL2_MAIN_LIBRARY)
    set(SDL2_MAIN_FOUND YES)
endif()

find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
    VERSION_VAR SDL2_VERSION
    HANDLE_COMPONENTS
)

if(SDL2_FOUND AND NOT TARGET SDL2)
    if(WIN32)
        list(PREPEND SDL2_LIBRARY imm32)
    endif()
    add_library(SDL2 INTERFACE IMPORTED GLOBAL)
    target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}/SDL2")
    target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
endif()

if(SDL2_MAIN_FOUND AND NOT TARGET SDL2main)
    if(WIN32)
        list(PREPEND SDL2_MAIN_LIBRARY mingw32)
    endif()
    add_library(SDL2main INTERFACE IMPORTED GLOBAL)
    target_link_libraries(SDL2main INTERFACE ${SDL2_MAIN_LIBRARY} ${SDL2_LIBRARY})
endif()

add_library(SDL2::SDL2 ALIAS SDL2)
add_library(SDL2::SDL2main ALIAS SDL2main)
