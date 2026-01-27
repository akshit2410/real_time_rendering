cmake_minimum_required(VERSION 3.10)
project(AssignmentOne)

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

add_executable(Assignment_one_main ${SOURCES})

# -----------------------------
# Link libraries
# -----------------------------
target_link_libraries(Assignment_one_main PRIVATE
    OpenGL::GL
    glfw
    assimp::assimp
    glad
)

# -----------------------------
# Copy resources
# -----------------------------
add_custom_command(TARGET Assignment_one_main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Assignment_one_main>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:Assignment_one_main>/resources"
)
