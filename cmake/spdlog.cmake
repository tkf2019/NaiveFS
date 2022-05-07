include(FetchContent)

if (${CMAKE_VERSION} VERSION_LESS 3.14)
  macro(FetchContent_MakeAvailable NAME)
  FetchContent_GetProperties(${NAME})
  if(NOT ${NAME}_POPULATED)
    FetchContent_Populate(${NAME})
    add_subdirectory(${${NAME}_SOURCE_DIR} ${${NAME}_BINARY_DIR})
  endif()
  endmacro()
  macro(FetchContent_MakeAvailable NAME)
    FetchContent_GetProperties(${NAME})
    if(NOT ${NAME}_POPULATED)
      FetchContent_Populate(${NAME})
      add_subdirectory(${${NAME}_SOURCE_DIR} ${${NAME}_BINARY_DIR})
    endif()
  endmacro()
endif()

set(SPDLOG_GIT_TAG v1.10.0)
# Very slow!
# set(SPDLOG_GIT_URL https://github.com/gabime/spdlog.git)
set(SPDLOG_GIT_URL git@github.com:gabime/spdlog.git)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY ${SPDLOG_GIT_URL}
  GIT_TAG ${SPDLOG_GIT_TAG}
)

FetchContent_MakeAvailable(spdlog)