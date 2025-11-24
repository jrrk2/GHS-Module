cmake_minimum_required(VERSION 3.15)
project(ImageQualityEvaluator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(-O3)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    if(CMAKE_BUILD_TYPE MATCHES "Release")
        add_compile_options(/O2)
    endif()
endif()

set(PCL_ROOT $ENV{PCL_ROOT})

message(STATUS "Looking for PCL in: ${PCL_ROOT}")

# PCL include directories
set(PCL_INCLUDE_DIR "${PCL_ROOT}/include")
if(NOT EXISTS ${PCL_INCLUDE_DIR})
    message(FATAL_ERROR "PCL include directory not found: ${PCL_INCLUDE_DIR}")
endif()

set(PCL_SRC_DIR "${PCL_ROOT}/src")
if(NOT EXISTS ${PCL_SRC_DIR})
    message(FATAL_ERROR "PCL source directory not found: ${PCL_SRC_DIR}")
endif()

# PCL library directory
set(PCL_LIB_DIR "${PCL_ROOT}/lib")
if(NOT EXISTS ${PCL_LIB_DIR})
    message(FATAL_ERROR "PCL library directory not found: ${PCL_LIB_DIR}")
endif()

# Find required libraries
find_library(PCL_LIBRARY NAMES PCL-pxi PATHS ${PCL_LIB_DIR} REQUIRED)
find_library(FFTW3_LIBRARY NAMES fftw3 REQUIRED)
find_library(FFTW3_THREADS_LIBRARY NAMES fftw3_threads REQUIRED)
find_library(CFITSIO_LIBRARY NAMES cfitsio REQUIRED)
find_library(LCMS2_LIBRARY NAMES lcms2 REQUIRED)
find_library(LZ4_LIBRARY lz4 PATHS /opt/homebrew/lib)
find_library(ZSTD_LIBRARY zstd PATHS /opt/homebrew/lib)
find_library(CRYPTO_LIBRARY crypto PATHS /opt/homebrew/lib)
find_library(TIFF_LIBRARY NAMES tiff PATHS /opt/homebrew/lib)

# Optional: Find zlib (often needed by PCL)
find_library(ZLIB_LIBRARY NAMES z)

message(STATUS "Found PCL library: ${PCL_LIBRARY}")
message(STATUS "Found FFTW3 library: ${FFTW3_LIBRARY}")
message(STATUS "Found FFTW3 threads library: ${FFTW3_THREADS_LIBRARY}")
message(STATUS "Found CFITSIO library: ${CFITSIO_LIBRARY}")
message(STATUS "Found LCMS2 library: ${LCMS2_LIBRARY}")
if(ZLIB_LIBRARY)
    message(STATUS "Found ZLIB library: ${ZLIB_LIBRARY}")
endif()

# Create library for the evaluator
add_library(ImageQualityEvaluator INTERFACE)
target_include_directories(ImageQualityEvaluator INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PCL_INCLUDE_DIR}
    ${PCL_SRC_DIR}/modules/file-formats
    /opt/homebrew/include
)

# Test executable
add_executable(test_evaluator test_evaluator.cpp ../PCLMockAPI.cpp ../PCLThreadMock.cpp)
target_link_libraries(test_evaluator PRIVATE
    ImageQualityEvaluator
    ${PCL_LIBRARY}
    ${LZ4_LIBRARY}
    ${ZSTD_LIBRARY} 
    ${CRYPTO_LIBRARY}
    ${TIFF_LIBRARY}
    /Users/jonathan/PCL/src/3rdparty/RFC6234/macosx/g++/arm64/Release/libRFC6234-pxi.a
    ${FFTW3_LIBRARY}
    ${FFTW3_THREADS_LIBRARY}
    ${CFITSIO_LIBRARY}
    ${LCMS2_LIBRARY}
)

if(ZLIB_LIBRARY)
    target_link_libraries(test_evaluator PRIVATE ${ZLIB_LIBRARY})
endif()

# Common PCL definitions
target_compile_definitions(test_evaluator PRIVATE
    _REENTRANT
    __PCL_NO_PERFORMANCE_CRITICAL_MATH_ROUTINES
)

# Platform-specific settings
if(APPLE)
    # macOS specific settings
    target_compile_definitions(test_evaluator PRIVATE __PCL_MACOSX)
    # Find required macOS frameworks
    find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    
    target_link_libraries(test_evaluator PRIVATE
        ${ACCELERATE_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )
    
    message(STATUS "Found Accelerate framework: ${ACCELERATE_FRAMEWORK}")
    message(STATUS "Found CoreFoundation framework: ${COREFOUNDATION_FRAMEWORK}")
    message(STATUS "Found Foundation framework: ${FOUNDATION_FRAMEWORK}")
    
elseif(UNIX AND NOT APPLE)
    # Linux specific settings
    target_compile_definitions(test_evaluator PRIVATE __PCL_LINUX)
    
    # Additional libraries that might be needed on Linux
    find_library(PTHREAD_LIBRARY pthread)
    find_library(DL_LIBRARY dl)
    find_library(M_LIBRARY m)
    
    if(PTHREAD_LIBRARY)
        target_link_libraries(test_evaluator PRIVATE ${PTHREAD_LIBRARY})
    endif()
    if(DL_LIBRARY)
        target_link_libraries(test_evaluator PRIVATE ${DL_LIBRARY})
    endif()
    if(M_LIBRARY)
        target_link_libraries(test_evaluator PRIVATE ${M_LIBRARY})
    endif()
    
elseif(WIN32)
    # Windows specific settings
    target_compile_definitions(test_evaluator PRIVATE __PCL_WINDOWS)
endif()

# Installation rules
install(TARGETS test_evaluator
    RUNTIME DESTINATION bin
)

install(FILES ImageQualityEvaluator.h
    DESTINATION include
)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  PCL root: ${PCL_ROOT}")
message(STATUS "  PCL source: ${PCL_SRC_DIR}")
message(STATUS "  PCL include: ${PCL_INCLUDE_DIR}")
message(STATUS "  PCL library: ${PCL_LIBRARY}")
message(STATUS "")

# Optional: Add a custom target to print help
add_custom_target(helpme
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  all            - Build all targets"
    COMMAND ${CMAKE_COMMAND} -E echo "  test_evaluator - Build the test executable"
    COMMAND ${CMAKE_COMMAND} -E echo "  install        - Install built targets"
    COMMAND ${CMAKE_COMMAND} -E echo "  clean          - Remove built files"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Usage:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake -DPCL_ROOT=/path/to/PCL -DCMAKE_BUILD_TYPE=Release .."
    COMMAND ${CMAKE_COMMAND} -E echo "  make"
    COMMAND ${CMAKE_COMMAND} -E echo "  ./test_evaluator image.xisf"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
