cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(VCT_RENDERER_SOURCES
	main.cpp
    )

set(SHADER_SOURCES 
	shaders/test.vert
    )

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(VCTRendererNoFramework ${VCT_RENDERER_SOURCES} ${SHADER_SOURCES}) 
#target_link_libraries(VCTRenderer dwSampleFramework)

foreach(GLSL ${SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/shaders/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/shaders"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -V ${GLSL} -o ${SPIRV} -gVS
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(VCTRenderer_Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(VCTRendererNoFramework VCTRenderer_Shaders)

set_property(TARGET VCTRendererNoFramework PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")