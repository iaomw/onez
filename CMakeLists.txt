cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(PROJECT_NAME onez)
project(${PROJECT_NAME} C CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()
message("${CMAKE_BUILD_TYPE} build")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(HAS_CLANGD "clangd")
if(HAS_CLANGD)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

file(GLOB sources CONFIGURE_DEPENDS *.h *.c *.cpp)
add_executable(${PROJECT_NAME} ${sources})

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})

find_package(glfw3 REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw)
target_include_directories(${PROJECT_NAME} PUBLIC ${GLFW_INCLUDE_DIRS})

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)

find_package(glslang REQUIRED)
target_link_libraries(${PROJECT_NAME} glslang glslang::SPIRV)

find_package(glm REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)

# find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
# target_include_directories(${PROJECT_NAME} PRIVATE ${STB_INCLUDE_DIRS})

# find_package(tinyobjloader CONFIG REQUIRED)
# target_link_libraries(${PROJECT_NAME} tinyobjloader::tinyobjloader)

# include(FetchContent)
# Set(FETCHCONTENT_QUIET OFF)

# FetchContent_Declare(
#   spirv-reflect
#   GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
#   GIT_TAG        sdk-1.3.224.1
#   SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/spirv-reflect
#   GIT_SHALLOW TRUE
#   GIT_PROGRESS TRUE
# )
# FetchContent_MakeAvailable(spirv-reflect)

# FetchContent_Declare(
#   glslang
#   GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
#   GIT_TAG        11.12.0
#   SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/external/glslang
#   GIT_SHALLOW TRUE
#   GIT_PROGRESS TRUE
# )
# FetchContent_MakeAvailable(glslang)

include_directories(external)

# add_subdirectory(external/SPIRV-Reflect)
# include_directories(external/SPIRV-Reflect)
# set_property(TARGET SPIRV-Reflect PROPERTY FOLDER "external/SPIRV-Reflect")

# add_subdirectory(external/volk)
include_directories(external/volk)
# target_link_libraries(${PROJECT_NAME} volk_headers)

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

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/textures/"
    COMMAND ${CMAKE_COMMAND} -E create_symlink #copy_directory
        "${CMAKE_SOURCE_DIR}/textures"
        "${CMAKE_BINARY_DIR}/textures"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/viking_room/"
    COMMAND ${CMAKE_COMMAND} -E create_symlink #copy_directory
        "${CMAKE_SOURCE_DIR}/viking_room"
        "${CMAKE_BINARY_DIR}/viking_room"
)

add_dependencies(${PROJECT_NAME} Shaders)

if(HAS_CLANGD)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink #copy_directory
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    )
endif()