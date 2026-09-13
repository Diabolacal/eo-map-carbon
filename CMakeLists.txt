# Copyright © 2026 Michael (eo-map-carbon experiment)
cmake_minimum_required(VERSION 3.31)

project(eo-map-carbon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Carbon's x64-windows-carbon toolchain always uses /MD, even for Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)

set(TRINITY_SOURCE_DIR "C:/dev/carbon-upstream/trinity" CACHE PATH "Checkout of carbonengine/trinity")
set(TRINITY_BUILD_DIR "${TRINITY_SOURCE_DIR}/.cmake-build-local-dx11-debug" CACHE PATH "Trinity CMake binary dir")
set(TRINITY_VCPKG_TRIPLET "x64-windows-debug" CACHE STRING "vcpkg triplet used to configure Trinity")

if(NOT EXISTS "${TRINITY_SOURCE_DIR}/trinityal/include/TrinityAL.h")
    message(FATAL_ERROR "Trinity source not found at ${TRINITY_SOURCE_DIR}. Clone https://github.com/carbonengine/trinity there first.")
endif()

set(_vcpkg_prefix "${TRINITY_BUILD_DIR}/vcpkg_installed/${TRINITY_VCPKG_TRIPLET}")
if(EXISTS "${_vcpkg_prefix}")
    list(PREPEND CMAKE_PREFIX_PATH "${_vcpkg_prefix}")
    list(PREPEND CMAKE_MODULE_PATH
        "${_vcpkg_prefix}/share/openvdb"
        "${_vcpkg_prefix}/share/boost"
    )
endif()

find_program(FXC_TOOL fxc
    HINTS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.17763.0/x64"
    REQUIRED
)

set(_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_generated_dir}")

function(compile_hlsl src profile header_name)
    set(_bin "${_generated_dir}/${header_name}.bin")
    set(_hdr "${_generated_dir}/${header_name}.h")
    add_custom_command(
        OUTPUT "${_hdr}"
        COMMAND "${FXC_TOOL}" /nologo /T${profile} /E main /Fo "${_bin}" "${src}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2h.py" "${_bin}" "${_hdr}"
        DEPENDS "${src}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2h.py"
        VERBATIM
    )
    set(${header_name}_HEADER "${_hdr}" PARENT_SCOPE)
endfunction()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/PositionOnly.vsh" vs_5_0 PositionOnly_vs)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/ConstantColor.psh" ps_5_0 ConstantColor_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/Starfield.vsh" vs_5_0 Starfield_vs)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/StarColor.psh" ps_5_0 StarColor_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/StarSprite.vsh" vs_5_0 StarSprite_vs)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/StarSprite.psh" ps_5_0 StarSprite_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/GateLine.vsh" vs_5_0 GateLine_vs)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/GateLine.psh" ps_5_0 GateLine_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/Fullscreen.vsh" vs_5_0 Fullscreen_vs)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/BloomExtract.psh" ps_5_0 BloomExtract_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/BloomBlur.psh" ps_5_0 BloomBlur_ps)
compile_hlsl("${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/BloomComposite.psh" ps_5_0 BloomComposite_ps)

# TrinityAL_dx11 is a static lib produced by the Trinity checkout.
set(_al_lib_debug "${TRINITY_BUILD_DIR}/carbon/autobuild/TrinityAL/Windows/x64/v141/TrinityAL_dx11_debug.lib")
set(_al_lib_release "${TRINITY_BUILD_DIR}/carbon/autobuild/TrinityAL/Windows/x64/v141/TrinityAL_dx11.lib")
if(EXISTS "${_al_lib_debug}")
    set(_al_lib "${_al_lib_debug}")
elseif(EXISTS "${_al_lib_release}")
    set(_al_lib "${_al_lib_release}")
else()
    set(_al_lib "${_al_lib_debug}")
    message(WARNING "TrinityAL_dx11.lib not built yet. Build TrinityAL_dx11 in ${TRINITY_BUILD_DIR} first.")
endif()

add_library(TrinityAL_dx11 STATIC IMPORTED)
set_target_properties(TrinityAL_dx11 PROPERTIES IMPORTED_LOCATION "${_al_lib}")

find_package(carbon-core CONFIG REQUIRED)
find_package(carbon-imageio CONFIG REQUIRED)

# These are PUBLIC deps of TrinityAL_dx11 in trinityal/CMakeLists.txt.
find_package(nvidia-aftermath CONFIG REQUIRED)
find_package(nvidia-streamline CONFIG REQUIRED)
find_package(nvidia-streamline-dlss-sr CONFIG REQUIRED)
find_package(nvidia-streamline-dlss-fg CONFIG REQUIRED)
find_package(nvidia-streamline-nis CONFIG REQUIRED)
find_package(amd-fidelityfx-fsr REQUIRED)

function(eo_map_carbon_app target)
    target_include_directories(${target} PRIVATE
        "${_generated_dir}"
        "${TRINITY_SOURCE_DIR}/trinityal"
        "${TRINITY_SOURCE_DIR}/trinityal/include"
    )
    target_compile_definitions(${target} PRIVATE
        TRINITY_PLATFORM=2
        TRINITY_AL_WITH_BLUE_EXPOSURE=0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_link_libraries(${target} PRIVATE
        TrinityAL_dx11
        CcpCore
        CcpImageIO
        d3d11
        dxgi
        dxguid
        NVIDIA::Aftermath
        NVIDIA::Streamline
        NVIDIA::Streamline::dlss-sr
        NVIDIA::Streamline::dlss-fg
        NVIDIA::Streamline::nis
        AMD::FidelityFX::fsr
        legacy_stdio_definitions
    )
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
    )
endfunction()

add_executable(eo-map-carbon-triangle WIN32
    src/triangle_main.cpp
    "${PositionOnly_vs_HEADER}"
    "${ConstantColor_ps_HEADER}"
)
eo_map_carbon_app(eo-map-carbon-triangle)

add_executable(eo-map-carbon-starfield WIN32
    src/starfield_main.cpp
    src/orbit_camera.h
    "${Starfield_vs_HEADER}"
    "${StarColor_ps_HEADER}"
)
eo_map_carbon_app(eo-map-carbon-starfield)

add_executable(eo-map-carbon-neweden WIN32
    src/neweden_main.cpp
    src/new_eden_catalog.cpp
    src/new_eden_catalog.h
    src/new_eden_gates.cpp
    src/new_eden_gates.h
    src/new_eden_gates_expect.h
    src/new_eden_star_visuals.cpp
    src/new_eden_star_visuals.h
    src/new_eden_anchors.h
    src/orbit_camera.h
    src/star_color.h
    src/tune_params.h
    src/tune_panel.cpp
    src/tune_panel.h
    "${Starfield_vs_HEADER}"
    "${StarColor_ps_HEADER}"
    "${StarSprite_vs_HEADER}"
    "${StarSprite_ps_HEADER}"
    "${GateLine_vs_HEADER}"
    "${GateLine_ps_HEADER}"
    "${Fullscreen_vs_HEADER}"
    "${BloomExtract_ps_HEADER}"
    "${BloomBlur_ps_HEADER}"
    "${BloomComposite_ps_HEADER}"
)
eo_map_carbon_app(eo-map-carbon-neweden)
target_link_libraries(eo-map-carbon-neweden PRIVATE comctl32)
add_custom_command(
    TARGET eo-map-carbon-neweden POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/new_eden_systems.bin"
        "$<TARGET_FILE_DIR:eo-map-carbon-neweden>/new_eden_systems.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/new_eden_stargates.bin"
        "$<TARGET_FILE_DIR:eo-map-carbon-neweden>/new_eden_stargates.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/data/new_eden_star_visuals.bin"
        "$<TARGET_FILE_DIR:eo-map-carbon-neweden>/new_eden_star_visuals.bin"
    COMMENT "Copy New Eden catalogue, stargate graph, and star visuals next to eo-map-carbon-neweden"
)
