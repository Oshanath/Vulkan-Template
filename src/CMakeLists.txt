cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
    message("Using 64-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message("Using 32-bit glslangValidator")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

set(VULKAN_TEMPLATE_SOURCES
	${PROJECT_SOURCE_DIR}/src/Application.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
    ${PROJECT_SOURCE_DIR}/src/TriangleRenderer.cpp
    ${PROJECT_SOURCE_DIR}/src/Model.cpp
    ${PROJECT_SOURCE_DIR}/src/Camera.cpp
    ${PROJECT_SOURCE_DIR}/src/Backend.cpp
    ${PROJECT_SOURCE_DIR}/src/CubeMap.cpp
    ${PROJECT_SOURCE_DIR}/src/Pipeline.cpp

    ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp
    )

set(SHADER_SOURCES 
	${PROJECT_SOURCE_DIR}/src/shaders/test.vert
    ${PROJECT_SOURCE_DIR}/src/shaders/test.frag
    ${PROJECT_SOURCE_DIR}/src/shaders/geometryPass.vert
    ${PROJECT_SOURCE_DIR}/src/shaders/geometryPass.frag
    ${PROJECT_SOURCE_DIR}/src/shaders/lightingPass.comp
    )

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/imgui
    ${PROJECT_SOURCE_DIR}/external/imgui/backends
)

add_executable(VulkanTemplate ${VULKAN_TEMPLATE_SOURCES} ${SHADER_SOURCES} "main.cpp")

foreach(GLSL ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/shaders"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.3 -V ${GLSL} -o ${SPIRV} -gVS
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(VulkanTemplate_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(VulkanTemplate VulkanTemplate_Shaders)

set_property(TARGET VulkanTemplate PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

target_link_libraries(VulkanTemplate glfw)
target_link_libraries(VulkanTemplate assimp)
target_link_libraries(VulkanTemplate ${Vulkan_LIBRARY})