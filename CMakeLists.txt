cmake_minimum_required(VERSION 2.8) 

set(OPENFX_PARENT_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
project(openfx)

# Add custom cmake macros
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(OpenFX)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
set_property(GLOBAL PROPERTY OPENFX_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}")

if("${OPENFX_PARENT_PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_SOURCE_DIR}")
    set(OPENFX_IS_MAIN YES)
else()
    set(OPENFX_IS_MAIN NO)
endif()

# The install rule does not depend on all, i.e. everything will not be built before installing
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

# CPP flags on debug / release mode
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DWINDOWS")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DWINDOWS")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Get OFX C headers
set(OFX_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
file(GLOB_RECURSE OFX_HEADER_FILES "${OFX_HEADER_DIR}/*.h")

# Install OFX C headers
install(
    FILES ${OFX_HEADER_FILES}
    DESTINATION "include"
)

# Build OFX C++ wrapper
add_subdirectory(Support)

if(OPENFX_IS_MAIN)
    add_subdirectory(HostSupport)
    add_subdirectory(Support/Plugins)
endif()
