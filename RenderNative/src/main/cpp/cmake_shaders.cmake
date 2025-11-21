
# Shader Compilation
find_program(GLSLC glslc HINTS ${ANDROID_NDK}/shader-tools/darwin-x86_64 ${ANDROID_NDK}/shader-tools/linux-x86_64 ${ANDROID_NDK}/shader-tools/windows-x86_64)

if (GLSLC)
    message(STATUS "Found glslc: ${GLSLC}")
    
    set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/shaders")
    set(SHADER_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders")
    
    file(MAKE_DIRECTORY ${SHADER_OUT_DIR})
    
    # Triangle Vert
    add_custom_command(
        OUTPUT ${SHADER_OUT_DIR}/triangle.vert.spv
        COMMAND ${GLSLC} -fshader-stage=vertex ${SHADER_SRC_DIR}/triangle.vert -o ${SHADER_OUT_DIR}/triangle.vert.spv
        DEPENDS ${SHADER_SRC_DIR}/triangle.vert
        COMMENT "Compiling triangle.vert"
    )
    
    # Triangle Frag
    add_custom_command(
        OUTPUT ${SHADER_OUT_DIR}/triangle.frag.spv
        COMMAND ${GLSLC} -fshader-stage=fragment ${SHADER_SRC_DIR}/triangle.frag -o ${SHADER_OUT_DIR}/triangle.frag.spv
        DEPENDS ${SHADER_SRC_DIR}/triangle.frag
        COMMENT "Compiling triangle.frag"
    )
    
    add_custom_target(compile_shaders ALL DEPENDS
        ${SHADER_OUT_DIR}/triangle.vert.spv
        ${SHADER_OUT_DIR}/triangle.frag.spv
    )
    
    add_dependencies(${CMAKE_PROJECT_NAME} compile_shaders)
else()
    message(WARNING "glslc not found! Shaders will not be compiled.")
endif()
