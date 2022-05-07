cmake_minimum_required(VERSION 3.23.0)
project(NaiveFS VERSION 0.0.1 LANGUAGES CXX)

# compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize-recover=address")
add_compile_options("-Wall" "-W" "-Wextra" "-fPIC" "-g3" "-D_FILE_OFFSET_BITS=64")

# outputs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# pkg-config fuse
find_package(PkgConfig)
pkg_search_module(FUSE REQUIRED fuse3)

message(STATUS "Found fuse libraries " ${FUSE_LIBRARIES})
message(STATUS "Found fuse include directory " ${FUSE_INCLUDE_DIRS})

set(NAIVEFS_INCLUDE_DIR include)
file(GLOB_RECURSE NAIVEFS_SOURCE src/*)
add_executable(${PROJECT_NAME} ${NAIVEFS_SOURCE})
target_include_directories(${PROJECT_NAME} PUBLIC ${NAIVEFS_INCLUDE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${FUSE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} INTERFACE pthread)
target_link_libraries(${PROJECT_NAME} PUBLIC ${FUSE_LIBRARIES})
