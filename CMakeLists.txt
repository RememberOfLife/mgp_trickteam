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

list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_LIST_DIR}/cmake")
find_package(mirabel REQUIRED) # use -DMIRABEL_ROOT=../.. if building from within plugins directory
# version check for mirabel apis
add_library(api_version_check OBJECT "src/static_version.cpp")
target_include_directories(api_version_check PRIVATE ${MIRABEL_INCLUDE_DIRS})

set(INCLUDES
    src
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


# build server
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
target_include_directories(mgp_tictactoe_server PUBLIC ${MIRABEL_INCLUDE_DIRS})
target_link_options(mgp_trickteam_server PRIVATE -rdynamic)
set_target_properties(mgp_trickteam_server PROPERTIES EXPORT_COMPILE_COMMANDS true)


# build client
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
target_include_directories(mgp_trickteam_client PUBLIC ${MIRABEL_INCLUDE_DIRS})
target_link_options(mgp_trickteam_client PRIVATE -rdynamic)
target_include_directories(mgp_trickteam_client PUBLIC ${OPENGL_INCLUDE_DIR})
include_directories(mgp_trickteam_client ${SDL2_INCLUDE_DIRS})
set_target_properties(mgp_trickteam_client PROPERTIES EXPORT_COMPILE_COMMANDS true)
