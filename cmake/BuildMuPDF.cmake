# cmake -P script: fetch MuPDF 1.28.0 into Compile/ and build static libs.

if(NOT PAPERCUT_MUPDF_SOURCE)
    message(FATAL_ERROR "PAPERCUT_MUPDF_SOURCE is required")
endif()
if(NOT PAPERCUT_MAKE)
    message(FATAL_ERROR "PAPERCUT_MAKE is required")
endif()
if(NOT PAPERCUT_MUPDF_JOBS)
    set(PAPERCUT_MUPDF_JOBS 4)
endif()

set(_src "${PAPERCUT_MUPDF_SOURCE}")
get_filename_component(_bin "${_src}" DIRECTORY)
set(_archive "${_bin}/mupdf-1.28.0-source.tar.gz")
set(_urls
    "https://mupdf.com/downloads/archive/mupdf-1.28.0-source.tar.gz"
    "https://mupdf.com/downloads/mupdf-1.28.0-source.tar.gz")

function(papercut_download_ok dest)
    if(NOT EXISTS "${dest}")
        set(_ok FALSE PARENT_SCOPE)
        return()
    endif()
    file(SIZE "${dest}" _sz)
    if(_sz GREATER 1000000)
        set(_ok TRUE PARENT_SCOPE)
    else()
        message(STATUS "Download too small (${_sz} bytes), ignoring")
        file(REMOVE "${dest}")
        set(_ok FALSE PARENT_SCOPE)
    endif()
endfunction()

function(papercut_download url dest)
    set(_ok FALSE PARENT_SCOPE)
    message(STATUS "Trying ${url}")
    foreach(_tls ON OFF)
        file(DOWNLOAD "${url}" "${dest}" SHOW_PROGRESS STATUS _st TIMEOUT 600 TLS_VERIFY ${_tls})
        list(GET _st 0 _code)
        if(_code EQUAL 0)
            papercut_download_ok("${dest}")
            if(_ok)
                set(_ok TRUE PARENT_SCOPE)
                return()
            endif()
        else()
            list(GET _st 1 _msg)
            message(STATUS "CMake download (TLS_VERIFY=${_tls}) failed: ${_msg}")
        endif()
    endforeach()
    find_program(_curl NAMES curl curl.exe)
    if(_curl)
        message(STATUS "Retrying with ${_curl}")
        execute_process(
            COMMAND "${_curl}" -L --retry 3 -o "${dest}" "${url}"
            RESULT_VARIABLE _cr)
        if(_cr EQUAL 0)
            papercut_download_ok("${dest}")
            if(_ok)
                set(_ok TRUE PARENT_SCOPE)
                return()
            endif()
        else()
            message(STATUS "curl failed (${_cr})")
        endif()
    endif()
endfunction()

if(NOT EXISTS "${_src}/include/mupdf/fitz.h")
    message(STATUS "Downloading MuPDF 1.28.0 into Compile/")
    set(_got FALSE)
    foreach(_url IN LISTS _urls)
        papercut_download("${_url}" "${_archive}")
        if(_ok)
            set(_got TRUE)
            break()
        endif()
    endforeach()
    if(NOT _got)
        message(FATAL_ERROR "Could not download MuPDF 1.28.0 source")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_bin}")
    if(NOT EXISTS "${_src}/include/mupdf/fitz.h")
        message(FATAL_ERROR "MuPDF archive did not extract to ${_src}")
    endif()
endif()

if(EXISTS "${_src}/build/release/libmupdf.a" AND EXISTS "${_src}/build/release/libmupdf-third.a")
    message(STATUS "MuPDF libraries already present in Compile/")
    return()
endif()

# MuPDF's makefile uses POSIX mkdir/bash. cmd.exe only echoes those recipes.
set(_make_env)
if(WIN32)
    find_program(PAPERCUT_SH NAMES sh
        HINTS
            "C:/Program Files/Git/usr/bin"
            "C:/Program Files (x86)/Git/usr/bin")
    if(NOT PAPERCUT_SH)
        message(FATAL_ERROR
            "MuPDF's makefile needs a Unix shell (sh, mkdir, bash). Install Git for Windows "
            "and keep its usr\\bin on PATH, then rebuild.")
    endif()
    get_filename_component(_sh_dir "${PAPERCUT_SH}" DIRECTORY)
    get_filename_component(_cc_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    set(ENV{PATH} "${_sh_dir};${_cc_dir};$ENV{PATH}")
    list(APPEND _make_env "SHELL=${PAPERCUT_SH}")
endif()

foreach(_d
        generated/resources/fonts/han
        generated/resources/fonts/droid
        generated/resources/fonts/noto
        generated/resources/fonts/sil
        generated/resources/fonts/urw
        build/release/source/fitz
        build/release/source/html
        build/release/source/pdf)
    file(MAKE_DIRECTORY "${_src}/${_d}")
endforeach()

set(_make_args
    -j${PAPERCUT_MUPDF_JOBS}
    build=release
    libs
    HAVE_X11=no
    HAVE_GLUT=no
    HAVE_CURL=no)
if(CMAKE_C_COMPILER)
    list(APPEND _make_args "CC=${CMAKE_C_COMPILER}")
endif()
if(CMAKE_CXX_COMPILER)
    list(APPEND _make_args "CXX=${CMAKE_CXX_COMPILER}")
endif()
if(WIN32)
    list(APPEND _make_args OS=MINGW64_NT XCFLAGS=-msse4.1 AR=ar "SHELL=${PAPERCUT_SH}")
endif()

message(STATUS "Building MuPDF with ${PAPERCUT_MAKE} ${_make_args}")
execute_process(
    COMMAND "${PAPERCUT_MAKE}" ${_make_args}
    WORKING_DIRECTORY "${_src}"
    RESULT_VARIABLE _rv)
if(NOT _rv EQUAL 0)
    message(FATAL_ERROR "MuPDF build failed (${_rv})")
endif()
if(NOT EXISTS "${_src}/build/release/libmupdf.a")
    message(FATAL_ERROR "MuPDF build did not produce libmupdf.a")
endif()
