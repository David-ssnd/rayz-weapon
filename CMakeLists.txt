cmake_minimum_required(VERSION 3.16.0)

# Include the shared component directory relative to this project
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(weapon)
