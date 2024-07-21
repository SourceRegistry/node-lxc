cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_CXX_STANDARD 17)

project(node-lxc)
include_directories(${CMAKE_JS_INC})
file(GLOB SOURCE_FILES "src/node-lxc.cpp" "src/Container.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

# Include Node-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE ${NODE_ADDON_API_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE "/usr/include/node")


# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=9)


## Add the directory containing lxc/lxccontainer.h to the include directories
#find_path(LXC_INCLUDE_DIR NAMES lxc/lxccontainer.h)
#include_directories(${LXC_INCLUDE_DIR})

# Find the LXC library
#find_library(LXC_LIBRARY NAMES lxc)
#
## Link the executable with the LXC library
#target_link_libraries(${PROJECT_NAME} ${LXC_LIBRARY})
#
## define NAPI_VERSION
#add_definitions(-DNAPI_VERSION=9)