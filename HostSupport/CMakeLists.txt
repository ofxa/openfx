set(OFX_HOSTSUPPORT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(OFX_HOSTSUPPORT_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE OFX_HOSTSUPPORT_HEADER_FILES "${OFX_HOSTSUPPORT_HEADER_DIR}/*.h")
file(GLOB_RECURSE OFX_HOSTSUPPORT_LIBRARY_FILES "${OFX_HOSTSUPPORT_LIBRARY_DIR}/*.cpp")

# Find EXPAT library
find_package(EXPAT)
if(NOT EXPAT_FOUND)
    message(STATUS "OfxHost: EXPAT not found. Please set EXPAT_INCLUDE_DIR and EXPAT_LIBRARY.")
else()
    # Create OfxHost static lib
    add_library(OfxHost STATIC ${OFX_HEADER_FILES} ${OFX_HOSTSUPPORT_HEADER_FILES} ${OFX_HOSTSUPPORT_LIBRARY_FILES})
    set_target_properties(OfxHost PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(OfxHost ${EXPAT_LIBRARY})
    if(NOT MSVC)
    	set_target_properties(OfxHost PROPERTIES COMPILE_FLAGS "-fPIC")
    endif()
    target_include_directories(OfxHost PUBLIC ${OFX_HEADER_DIR} ${OFX_HOSTSUPPORT_HEADER_DIR} ${EXPAT_INCLUDE_DIR})

    # Install OfxHost headers and library
    install(
        FILES ${OFX_HOSTSUPPORT_HEADER_FILES}
        DESTINATION "include"
    )
    install(TARGETS OfxHost DESTINATION "lib" OPTIONAL)
endif()
