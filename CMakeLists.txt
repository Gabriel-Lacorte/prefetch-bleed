cmake_minimum_required(VERSION 3.26)

project(prefetch-bleed
    VERSION     1.0.0
    LANGUAGES   CXX ASM_MASM
    DESCRIPTION "Windows KASLR break via prefetch side-channel")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "prefetch-bleed targets x86_64 only.")
endif()

set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_executable(prefetch-bleed
    src/demo.cpp
    src/kaslr.asm)

target_include_directories(prefetch-bleed PRIVATE src)

target_link_libraries(prefetch-bleed PRIVATE ntdll)

target_compile_definitions(prefetch-bleed PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE)

if(MSVC)
    target_compile_options(prefetch-bleed PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std:c++latest>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:inline>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
        $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
        $<$<COMPILE_LANGUAGE:CXX>:/GR->
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/O2>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Oi>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/GL>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Gy>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/Gw>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/guard:cf>)

    target_link_options(prefetch-bleed PRIVATE
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
        /DYNAMICBASE
        /HIGHENTROPYVA
        /NXCOMPAT
        /GUARD:CF
        /CETCOMPAT)
endif()

set_target_properties(prefetch-bleed PROPERTIES
    OUTPUT_NAME          prefetch-bleed
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
