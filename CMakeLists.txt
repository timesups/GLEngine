cmake_minimum_required(VERSION 3.20)

project(GLEngineNew LANGUAGES C CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CPP_LIBRARY_ROOT "C:/CPP_Library" CACHE PATH "Root of prebuilt third-party libraries (include, libs, bin)")

if(NOT EXISTS "${CPP_LIBRARY_ROOT}/include")
    message(FATAL_ERROR "CPP_LIBRARY_ROOT is invalid: ${CPP_LIBRARY_ROOT}/include not found")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wno-unknown-warning-option)
    foreach(_flag_var CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
        if("${${_flag_var}}" MATCHES "-utf-8")
            string(REPLACE "-utf-8" "-finput-charset=UTF-8" ${_flag_var} "${${_flag_var}}")
            set(${_flag_var} "${${_flag_var}}" CACHE STRING "Flags used by the compiler during all build types." FORCE)
        endif()
    endforeach()
elseif(MSVC)
    add_compile_options(/utf-8)
endif()

set(GLEngineNew_SOURCES
    src/main.cpp
    src/Engine/Core/Config.cpp
    src/Engine/Core/Gui.cpp
    src/Engine/Asset/LoaderManager.cpp
    src/Engine/Core/Log.cpp
    src/Engine/Core/util.cpp
    src/Engine/Core/Window.cpp
    src/Engine/Entity/Entity.cpp
    src/Engine/Entity/EntityManager.cpp
    src/Engine/Entity/RenderUnitFilter.cpp
    src/Engine/Scene/SceneSession.cpp
    src/Engine/Scene/SceneSerializer.cpp
    src/Engine/Entity/Components/Camera.cpp
    src/Engine/Entity/Components/Component.cpp
    src/Engine/Entity/Components/Light.cpp
    src/Engine/Entity/Components/MeshRender.cpp
    src/Engine/Entity/Components/SkyBox.cpp
    src/Engine/Entity/Components/Transform.cpp
    src/Engine/Asset/AssetManager.cpp
    src/Engine/Asset/AssetDatabase.cpp
    src/Engine/Asset/AssetPaths.cpp
    src/Engine/Renderer/FramebufferDesc.cpp
    src/Engine/Renderer/RenderBuffer.cpp
    src/Engine/Renderer/FramebufferBinding.cpp
    src/Engine/Renderer/RenderTarget.cpp
    src/Engine/Renderer/CubemapBaker.cpp
    src/Engine/Renderer/Renderer.cpp
    src/Engine/Renderer/RenderContext.cpp
    src/Engine/Renderer/InstanceBuffer.cpp
    src/Engine/Renderer/DrawBatch.cpp
    src/Engine/Renderer/UniformBuffer.cpp
    src/Engine/Asset/Types/Material.cpp
    src/Engine/Asset/Types/Mesh.cpp
    src/Engine/Asset/Types/Model.cpp
    src/Engine/Asset/Types/RenderQueue.cpp
    src/Engine/Asset/Types/Shader.cpp
    src/Engine/Asset/Types/Texture/Texture.cpp
    src/Engine/Asset/Types/Texture/Texture2D.cpp
    src/Engine/Asset/Types/Texture/TextureCube.cpp
    src/Engine/Asset/Types/Texture/TextureCubeArray.cpp
    src/Engine/Asset/Types/Texture/Texture2DArray.cpp
    src/Engine/Asset/Types/ShaderPass.cpp
    src/Engine/Renderer/RenderPipeline/DeferredRender.cpp
    src/Engine/Renderer/RenderPipeline/DeferredPlusRender.cpp
    src/Engine/Renderer/RenderPipeline/ForwardRender.cpp
    src/Engine/Renderer/RenderPipeline/RenderPipeline.cpp
    src/Engine/Renderer/RenderPipeline/RenderPipelineRegistry.cpp
    external/glad/glad.c
    external/stb/stb_image.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

add_executable(GLEngineNew ${GLEngineNew_SOURCES})

target_sources(GLEngineNew PRIVATE
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/src/Engine/Core/RenderDocSupport.cpp>"
)

target_compile_definitions(GLEngineNew PRIVATE
    "$<$<CONFIG:Debug>:GLE_ENABLE_RENDERDOC=1>"
)

set_target_properties(GLEngineNew PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/$<CONFIG>"
)

target_include_directories(GLEngineNew PRIVATE
    "${CPP_LIBRARY_ROOT}/include"
    "${CMAKE_SOURCE_DIR}/external"
    "${CMAKE_SOURCE_DIR}/external/imgui"
    "${CMAKE_SOURCE_DIR}/external/imgui/backends"
    "$<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/external/renderdoc>"
    "$<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/resources>"
)

if(WIN32)
    target_sources(GLEngineNew PRIVATE "${CMAKE_SOURCE_DIR}/resources/app.rc")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/resources/app.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/resources/icon/greenland.ico"
    )
endif()

target_link_directories(GLEngineNew PRIVATE "${CPP_LIBRARY_ROOT}/libs")

set(GLEngineNew_ASSIMP_RELEASE_DLL assimp-vc143-mt.dll)
set(GLEngineNew_ASSIMP_RELEASE_LIB assimp-vc143-mt)
if(NOT EXISTS "${CPP_LIBRARY_ROOT}/libs/assimp-vc143-mt.lib")
    set(GLEngineNew_ASSIMP_RELEASE_DLL assimp-vc143-mtd.dll)
    set(GLEngineNew_ASSIMP_RELEASE_LIB assimp-vc143-mtd)
    message(STATUS "assimp-vc143-mt.lib not found; Release will use assimp-vc143-mtd")
endif()

target_link_libraries(GLEngineNew PRIVATE
    opengl32
    unit
    "$<$<CONFIG:Debug>:glfw3D>"
    "$<$<NOT:$<CONFIG:Debug>>:glfw3>"
    "$<$<CONFIG:Debug>:assimp-vc143-mtd>"
    "$<$<NOT:$<CONFIG:Debug>>:${GLEngineNew_ASSIMP_RELEASE_LIB}>"
)

add_custom_command(TARGET GLEngineNew POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<$<CONFIG:Debug>:${CPP_LIBRARY_ROOT}/bin/assimp-vc143-mtd.dll>$<$<NOT:$<CONFIG:Debug>>:${CPP_LIBRARY_ROOT}/bin/${GLEngineNew_ASSIMP_RELEASE_DLL}>"
        "$<TARGET_FILE_DIR:GLEngineNew>"
    COMMENT "Copying runtime DLLs next to the executable"
)
