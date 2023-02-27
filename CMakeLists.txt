cmake_minimum_required(VERSION 3.18)
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)

project(mgp_trickteam C CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if(NOT CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg].*")
    if(supported)
        message(STATUS "IPO/LTO enabled")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO/LTO not supported: <${error}>")
    endif()
else()
    message(STATUS "IPO/LTO disabled in non-release build")
endif()

find_package(OpenGL REQUIRED)

find_package(SDL2 REQUIRED)

set(INCLUDES_NANOVG
    lib/mirabel/lib
)

set(INCLUDES
    lib/mirabel/lib/nanovg/src
    lib/mirabel/lib/imgui
    lib/rosalia/includes # we use our own static rosalia
    lib/mirabel/lib/surena/includes
    lib/mirabel/includes
    src
    "${CMAKE_BINARY_DIR}/build_generated"
)

set(SOURCES_SERVER
    src/game.cpp

    src/game_plugin.c
)

set(SOURCES_CLIENT
    src/frontend.cpp
    src/game.cpp

    src/frontend_plugin.c
    src/game_plugin.c
)

add_library(mgp_trickteam_server SHARED "${SOURCES_SERVER}")
set_target_properties(mgp_trickteam_server PROPERTIES PREFIX "")
set_target_properties(mgp_trickteam_server PROPERTIES LIBRARY_OUTPUT_NAME trickteam_server)
target_compile_options(mgp_trickteam_server PRIVATE
    # "-Wall"
    # "-Wextra"
    # "-Werror" # warnings as errors
    "-Wfatal-errors" # stop after first error
)
target_include_directories(mgp_trickteam_server PRIVATE ${INCLUDES})
target_link_options(mgp_trickteam_server PRIVATE -rdynamic)
set_target_properties(mgp_trickteam_server PROPERTIES EXPORT_COMPILE_COMMANDS true)

add_library(mgp_trickteam_client SHARED "${SOURCES_CLIENT}")
set_target_properties(mgp_trickteam_client PROPERTIES PREFIX "")
set_target_properties(mgp_trickteam_client PROPERTIES LIBRARY_OUTPUT_NAME trickteam_client)
target_compile_options(mgp_trickteam_client PRIVATE
    # "-Wall"
    # "-Wextra"
    # "-Werror" # warnings as errors
    "-Wfatal-errors" # stop after first error
)
target_include_directories(mgp_trickteam_client PRIVATE ${INCLUDES})
target_link_options(mgp_trickteam_client PRIVATE -rdynamic)
target_include_directories(mgp_trickteam_client PUBLIC ${OPENGL_INCLUDE_DIR})
include_directories(mgp_trickteam_client ${SDL2_INCLUDE_DIRS})
set_target_properties(mgp_trickteam_client PROPERTIES EXPORT_COMPILE_COMMANDS true)
