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

# ── node-addon-api ─────────────────────────────────────────────────────────────
# Always lives in the project's own node_modules — no PATH dependency.
set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api")
if(NOT EXISTS "${NODE_ADDON_API_DIR}/napi.h")
    # Fallback: ask node (works in shell-launched cmake)
    execute_process(
        COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _naa_out
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    string(REGEX REPLACE "\"" "" NODE_ADDON_API_DIR "${_naa_out}")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE "${NODE_ADDON_API_DIR}")

# ── Node.js headers (node_api.h, uv.h, …) ─────────────────────────────────────
# Strategy 1: derive from the node binary on PATH
execute_process(
    COMMAND node -e "process.stdout.write(process.execPath)"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE _node_exe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(_node_exe)
    get_filename_component(_node_bin "${_node_exe}" DIRECTORY)
    get_filename_component(_node_root "${_node_bin}" DIRECTORY)
    list(APPEND _node_include_hints "${_node_root}/include/node")
endif()

# Strategy 2: glob the node-gyp header cache (works even when node is not on PATH)
file(GLOB _node_gyp_dirs
    "$ENV{HOME}/.node-gyp/*/include/node"
    "/.jbdevcontainer/node-gyp/*/include/node"
)
list(APPEND _node_include_hints ${_node_gyp_dirs})

find_path(NODE_INCLUDE_DIR
    NAMES node_api.h uv.h
    HINTS ${_node_include_hints}
    PATHS /usr/include/node /usr/local/include/node
    NO_DEFAULT_PATH
)

if(NODE_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE "${NODE_INCLUDE_DIR}")
else()
    message(WARNING "Node.js headers not found. Run 'npx node-gyp configure' once to populate the cache.")
endif()

# ── cmake-js (only set when building via cmake-js) ────────────────────────────
if(CMAKE_JS_INC)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_JS_INC})
endif()
if(CMAKE_JS_LIB)
    target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})
endif()

# ── LXC ───────────────────────────────────────────────────────────────────────
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

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(${PROJECT_NAME} PRIVATE NAPI_VERSION=9)
