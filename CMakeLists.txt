cmake_minimum_required(VERSION 3.15)
project(astara
        VERSION 0.2.0
        DESCRIPTION "PWR primary-loop dynamic simulator"
        LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Build configuration
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Sanity: forbid in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Run cmake from a build/ directory.")
endif()

option(ASTARA_BUILD_TESTS    "Build unit + analytical tests" ON)
option(ASTARA_BUILD_EXAMPLES "Build example programs"        ON)
option(ASTARA_BUILD_DOCS     "Build Doxygen documentation"   OFF)
option(ASTARA_ENABLE_OPENMP  "Enable OpenMP parallelism"     ON)
option(ASTARA_DEBUG_CHECKS   "Enable per-step conservation residual checks" OFF)

# -----------------------------------------------------------------------------
# Compiler warnings (interface target so all our targets pick them up)
# -----------------------------------------------------------------------------
add_library(astara_warnings INTERFACE)
target_compile_options(astara_warnings INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall -Wextra -Wpedantic
            -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align
            -Wunused -Woverloaded-virtual -Wnull-dereference
            -Wdouble-promotion -Wformat=2>
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4 /permissive->)

# -----------------------------------------------------------------------------
# OpenMP
# -----------------------------------------------------------------------------
if(ASTARA_ENABLE_OPENMP)
    find_package(OpenMP REQUIRED)
endif()

# -----------------------------------------------------------------------------
# Water properties: IF97 (header-only IAPWS-IF97, ships with CoolProp)
# -----------------------------------------------------------------------------
set(IF97_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals//IF97")
if(NOT EXISTS "${IF97_DIR}/IF97.h")
    message(FATAL_ERROR
            "IF97 header not found at ${IF97_DIR}/IF97.h.\n"
            "Run: git submodule update --init externals/IF97")
endif()
add_library(astara_if97 INTERFACE)
target_include_directories(astara_if97 SYSTEM INTERFACE "${IF97_DIR}")

# -----------------------------------------------------------------------------
# Project sources
# -----------------------------------------------------------------------------
add_subdirectory(src)

if(ASTARA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ASTARA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# -----------------------------------------------------------------------------
# Doxygen
# -----------------------------------------------------------------------------
if(ASTARA_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(DOXYGEN_IN  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
    set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
    configure_file("${DOXYGEN_IN}" "${DOXYGEN_OUT}" @ONLY)
    add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} "${DOXYGEN_OUT}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Generating Doxygen documentation"
            VERBATIM)
endif()

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "ASTARA configuration:")
message(STATUS "  build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenMP            : ${ASTARA_ENABLE_OPENMP}")
message(STATUS "  tests             : ${ASTARA_BUILD_TESTS}")
message(STATUS "  examples          : ${ASTARA_BUILD_EXAMPLES}")
message(STATUS "  docs              : ${ASTARA_BUILD_DOCS}")
message(STATUS "  conservation chk  : ${ASTARA_DEBUG_CHECKS}")
