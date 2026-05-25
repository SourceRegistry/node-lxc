cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_CXX_STANDARD 17)

project(node-lxc)

# All C++ sources and headers (glob recurse so IDEs index every file)
file(GLOB_RECURSE SOURCE_FILES
    "src/*.cpp"
    "src/*.h"
)
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Own src/ directory — resolves #include "Container.h" and #include "helpers/..."
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")

# Node.js headers — derive from whichever node is on PATH
execute_process(
    COMMAND node -e "process.stdout.write(process.execPath)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_EXECUTABLE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
get_filename_component(NODE_BIN_DIR  "${NODE_EXECUTABLE}" DIRECTORY)
get_filename_component(NODE_ROOT     "${NODE_BIN_DIR}"    DIRECTORY)
target_include_directories(${PROJECT_NAME} PRIVATE "${NODE_ROOT}/include/node")

# node-addon-api headers
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REGEX REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${NODE_ADDON_API_DIR}")

# cmake-js provided includes and lib (only set when building via cmake-js)
if(CMAKE_JS_INC)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})
endif()
if(CMAKE_JS_LIB)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})
endif()

# LXC headers and library
find_path(LXC_INCLUDE_DIR
    NAMES lxc/lxccontainer.h
    PATHS /usr/include /usr/local/include
)
find_library(LXC_LIBRARY
    NAMES lxc
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
)

if(LXC_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE ${LXC_INCLUDE_DIR})
else()
    message(WARNING "liblxc-dev not found — install with: sudo apt install lxc-dev")
endif()

if(LXC_LIBRARY)
    target_link_libraries(${PROJECT_NAME} ${LXC_LIBRARY})
else()
    message(WARNING "liblxc not found — install with: sudo apt install lxc")
endif()

# Mirror the binding.gyp feature detection for set_timeout
if(LXC_INCLUDE_DIR)
    file(READ "${LXC_INCLUDE_DIR}/lxc/lxccontainer.h" _lxc_header)
    if(_lxc_header MATCHES "set_timeout")
        target_compile_definitions(${PROJECT_NAME} PRIVATE LXC_HAS_SET_TIMEOUT)
    endif()
endif()

# NAPI version
target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_VERSION=9)
