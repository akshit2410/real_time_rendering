cmake_minimum_required(VERSION 3.10)
project(Assignmentfive)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------
# OpenGL
# -----------------------------
find_package(OpenGL REQUIRED)

# -----------------------------
# GLFW
# -----------------------------
find_package(glfw3 3.3 REQUIRED)

# -----------------------------
# GLM
# -----------------------------
find_path(GLM_INCLUDE_DIR glm/glm.hpp)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM not found. Install via: brew install glm")
endif()

# -----------------------------
# Assimp
# -----------------------------
find_package(assimp REQUIRED)

# -----------------------------
# GLAD (local)
# -----------------------------
set(GLAD_ROOT "${CMAKE_CURRENT_LIST_DIR}/external/glad-3.3")
set(GLAD_INC "${GLAD_ROOT}/include")

add_library(glad "${GLAD_ROOT}/src/glad.c")
target_include_directories(glad PUBLIC "${GLAD_INC}")

# -----------------------------
# ImGui (STATIC LIBRARY)
# -----------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_LIST_DIR}/external/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# -----------------------------
# Include directories
# -----------------------------
include_directories(
    ${GLAD_INC}
    ${GLM_INCLUDE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# -----------------------------
# Sources
# -----------------------------
file(GLOB SOURCES "source_code/*.cpp")

add_executable(Assignment_five_main ${SOURCES})

# -----------------------------
# Link libraries
# -----------------------------
target_link_libraries(Assignment_five_main PRIVATE
    OpenGL::GL
    glfw
    assimp::assimp
    glad
    imgui
)


