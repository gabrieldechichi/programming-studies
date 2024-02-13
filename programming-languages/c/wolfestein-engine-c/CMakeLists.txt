cmake_minimum_required(VERSION "3.19.2")

project("wolfestein")

set(SDL_STATIC ON CACHE BOOL "Build SDL2 static library")
set(SDL_SHARED OFF CACHE BOOL "Build SDL2 shared library")
# Include SDL2 source as a subdirectory
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/SDL EXCLUDE_FROM_ALL)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable("wolfestein" "${MY_SOURCES}")

# Set include directories on the target
target_include_directories(wolfestein PRIVATE 
    ${CMAKE_SOURCE_DIR}/vendor/linmath.h
    ${CMAKE_SOURCE_DIR}/vendor/klib
    ${CMAKE_SOURCE_DIR}/vendor/SDL/include
)

target_link_libraries(wolfestein PRIVATE SDL3-static)