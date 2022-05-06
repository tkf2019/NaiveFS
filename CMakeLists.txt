cmake_minimum_required(VERSION 3.2)
project(naivefs)

IF (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF()
add_compile_options(-Wall -Wextra -fexceptions)
IF (CMAKE_BUILD_TYPE STREQUAL Debug)
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

find_package(FUSE REQUIRED)

aux_source_directory(${PROJECT_SOURCE_DIR}/src/ PROJ_SOURCES)
add_executable(${PROJECT_NAME} ${PROJ_SOURCES})
include_directories(${PROJECT_SOURCE_DIR}/include ${FUSE_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${FUSE_LIBRARIES} pthread)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)