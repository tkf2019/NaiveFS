cmake_minimum_required(VERSION 3.2)
project(thrid_party)
include(ExternalProject)

ExternalProject_Add(spdlog
    GIT_REPOSITORY "https://github.com/gabime/spdlog"
    GIT_TAG "v1.10.0"
    PREFIX "thirdparty"
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=Release
)
