cmake_minimum_required(VERSION 3.31)
project(ipc_demo_server)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(ipc_demo_server ${SRC_FILES})

# ---------------------------------------------------------------------------
# Lire le contenu du fichier VERSION
file(READ "${PROJECT_SOURCE_DIR}/VERSION" PROJECT_VERSION_RAW)
string(STRIP "${PROJECT_VERSION_RAW}" PROJECT_VERSION)   # enlève \n

message(STATUS "Project version: ${PROJECT_VERSION}")

# Créer un header Version.hpp à partir d’un template
configure_file(
        ${PROJECT_SOURCE_DIR}/include/Version.hpp.in
        ${CMAKE_BINARY_DIR}/generated/Version.hpp
        @ONLY
)
# ---------------------------------------------------------------------------

target_include_directories(ipc_demo_server
        PUBLIC              # ou PRIVATE / INTERFACE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/json/single_include
        ${CMAKE_BINARY_DIR}/generated
)
