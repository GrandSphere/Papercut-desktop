# MuPDF 1.28.0: vendored in Compile/ or USE_SYSTEM_MUPDF.

if(USE_SYSTEM_MUPDF)
    find_path(MUPDF_INCLUDE_DIR mupdf/fitz.h)
    find_library(MUPDF_LIBRARY mupdf)
    if(NOT MUPDF_INCLUDE_DIR OR NOT MUPDF_LIBRARY)
        message(FATAL_ERROR "USE_SYSTEM_MUPDF=ON but libmupdf was not found")
    endif()
    add_library(mupdf::mupdf UNKNOWN IMPORTED)
    set_target_properties(mupdf::mupdf PROPERTIES
        IMPORTED_LOCATION "${MUPDF_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MUPDF_INCLUDE_DIR}")
    return()
endif()

set(PAPERCUT_MUPDF_SOURCE "${CMAKE_BINARY_DIR}/mupdf-1.28.0-source")
set(MUPDF_INCLUDE_DIR "${PAPERCUT_MUPDF_SOURCE}/include")
set(MUPDF_LIBRARY "${PAPERCUT_MUPDF_SOURCE}/build/release/libmupdf.a")
set(MUPDF_THIRD_LIBRARY "${PAPERCUT_MUPDF_SOURCE}/build/release/libmupdf-third.a")
# Imported INTERFACE_INCLUDE_DIRECTORIES must exist at generate time, before the tarball is fetched.
file(MAKE_DIRECTORY "${MUPDF_INCLUDE_DIR}")

find_program(PAPERCUT_MAKE NAMES mingw32-make make gmake REQUIRED)
include(ProcessorCount)
ProcessorCount(PAPERCUT_MUPDF_JOBS)
if(PAPERCUT_MUPDF_JOBS EQUAL 0)
    set(PAPERCUT_MUPDF_JOBS 4)
endif()

add_custom_command(
    OUTPUT "${MUPDF_LIBRARY}" "${MUPDF_THIRD_LIBRARY}"
    COMMAND ${CMAKE_COMMAND}
        "-DPAPERCUT_MUPDF_SOURCE=${PAPERCUT_MUPDF_SOURCE}"
        "-DPAPERCUT_MAKE=${PAPERCUT_MAKE}"
        "-DPAPERCUT_MUPDF_JOBS=${PAPERCUT_MUPDF_JOBS}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DWIN32=${WIN32}"
        -P "${CMAKE_SOURCE_DIR}/cmake/BuildMuPDF.cmake"
    COMMENT "Downloading/building MuPDF 1.28.0 into Compile/"
    VERBATIM)

add_custom_target(mupdf_build DEPENDS "${MUPDF_LIBRARY}" "${MUPDF_THIRD_LIBRARY}")

add_library(mupdf::mupdf STATIC IMPORTED GLOBAL)
set_target_properties(mupdf::mupdf PROPERTIES
    IMPORTED_LOCATION "${MUPDF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MUPDF_INCLUDE_DIR}")
set_property(TARGET mupdf::mupdf APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES "${MUPDF_THIRD_LIBRARY}")
add_dependencies(mupdf::mupdf mupdf_build)
