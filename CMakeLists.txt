cmake_minimum_required(VERSION 3.2)
project(naivefs VERSION 0.0.1 LANGUAGES CXX)

# compiler
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall --pedantic -g")

# outputs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# pkg-config fuse
find_package(PkgConfig)
pkg_search_module(FUSE REQUIRED fuse3)
message(STATUS ${FUSE_LIBRARIES})
message(STATUS ${FUSE_INCLUDE_DIRS})

aux_source_directory(${PROJECT_SOURCE_DIR}/src/ PROJ_SOURCES)
add_executable(${PROJECT_NAME} ${PROJ_SOURCES})
include_directories(
  ${PROJECT_SOURCE_DIR}/include
  ${FUSE_INCLUDE_DIRS}
)
link_directories(${FUSE_LIBRARY_DIR})
target_link_libraries(${PROJECT_NAME} ${FUSE_LIBRARIES} pthread)

# add thirdparty dependencies
if (NOT EXISTS ${CMAKE_BINARY_DIR}/thirdparty)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/thirdparty)
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}/thirdparty
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/thirdparty
)

execute_process(
    COMMAND ${CMAKE_COMMAND} --build . -j
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/thirdparty
)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_BINARY_DIR}/thirdparty/thirdparty)

find_package(spdlog REQUIRED)