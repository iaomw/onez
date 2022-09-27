cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_NAME vktest)
project(${PROJECT_NAME})

#set(CMAKE_BUILD_TYPE Debug)

add_executable(vktest)
file(GLOB sources CONFIGURE_DEPENDS *.h *.cpp)
target_sources(vktest PUBLIC main.cpp)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

# target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
# target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} glfw vulkan)
# target_include_directories(${PROJECT_NAME} PUBLIC ${GLFW_INCLUDE_DIRS})

include_directories(external)
#include_directories(external/glm)
#include_directories(external/gli)
#include_directories(external/imgui)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "shaders/*.frag"
    "shaders/*.vert"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders/"
        COMMAND glslc ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_custom_command(TARGET vktest POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/textures/"
    COMMAND ${CMAKE_COMMAND} -E create_symlink #copy_directory
        "${CMAKE_SOURCE_DIR}/textures"
        "${CMAKE_BINARY_DIR}/textures"
)

add_custom_command(TARGET vktest POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/viking_room/"
    COMMAND ${CMAKE_COMMAND} -E create_symlink #copy_directory
        "${CMAKE_SOURCE_DIR}/viking_room"
        "${CMAKE_BINARY_DIR}/viking_room"
)

add_dependencies(vktest Shaders)