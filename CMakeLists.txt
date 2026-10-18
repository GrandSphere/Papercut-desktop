cmake_minimum_required(VERSION 3.22)

# Pick the MinGW that ships with Qt when the caller has not already chosen a compiler.
if(WIN32 AND NOT CMAKE_C_COMPILER)
    foreach(_mingw
            "C:/Qt/Tools/mingw1310_64/bin"
            "C:/Qt/Tools/mingw1120_64/bin")
        if(EXISTS "${_mingw}/gcc.exe")
            set(CMAKE_C_COMPILER "${_mingw}/gcc.exe")
            set(CMAKE_CXX_COMPILER "${_mingw}/g++.exe")
            break()
        endif()
    endforeach()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    message(FATAL_ERROR "VERSION file missing at repository root")
endif()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _papercut_version_lines)
list(LENGTH _papercut_version_lines _papercut_version_n)
if(_papercut_version_n LESS 2)
    message(FATAL_ERROR "VERSION file must contain two lines: version and integer version code")
endif()
list(GET _papercut_version_lines 0 PAPERCUT_VERSION_STRING)
list(GET _papercut_version_lines 1 PAPERCUT_VERSION_CODE)
if(PAPERCUT_VERSION_STRING STREQUAL "")
    message(FATAL_ERROR "VERSION file: first line must be the release version (e.g. 0.146)")
endif()
if(NOT PAPERCUT_VERSION_CODE MATCHES "^[0-9]+$")
    message(FATAL_ERROR "VERSION file: second line must be the integer version code")
endif()

project(papercut VERSION ${PAPERCUT_VERSION_STRING} LANGUAGES C CXX)

configure_file(
    "${CMAKE_SOURCE_DIR}/src/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
    @ONLY)

if(MSVC)
    message(FATAL_ERROR "Papercut on Windows is built with MinGW, not MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(USE_SYSTEM_MUPDF "Link a system libmupdf instead of downloading 1.28.0 into Compile/" OFF)
option(PAPERCUT_BUILD_GUI "Build the Qt Quick GUI" ON)
option(PAPERCUT_PACKAGE_RELEASE "Copy a ready-to-run tree into Source/Release/<OS> after compile" ON)

# Local Qt install; GitHub Actions already sets CMAKE_PREFIX_PATH / Qt6_DIR.
if(WIN32)
    if(NOT CMAKE_PREFIX_PATH AND NOT DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT DEFINED Qt6_DIR)
        foreach(_qt "C:/Qt/6.8.3/mingw_64" "C:/Qt/6.8.2/mingw_64" "C:/Qt/6.9.0/mingw_64")
            if(EXISTS "${_qt}/lib/cmake/Qt6/Qt6Config.cmake")
                list(APPEND CMAKE_PREFIX_PATH "${_qt}")
                break()
            endif()
        endforeach()
    endif()
    foreach(_bin "C:/Qt/Tools/mingw1310_64/bin" "C:/Qt/6.8.3/mingw_64/bin")
        if(EXISTS "${_bin}")
            set(ENV{PATH} "${_bin};$ENV{PATH}")
        endif()
    endforeach()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Sql)

include("${CMAKE_SOURCE_DIR}/cmake/FindMuPDF.cmake")

add_library(papercut-core STATIC
    src/core/AppPaths.cpp
    src/core/Io.cpp
    src/core/Keybinds.cpp
    src/cli/main.cpp
    src/data/SettingsRepository.cpp
    src/data/BackupJson.cpp
    src/data/BookmarkRepository.cpp
    src/data/Database.cpp
    src/data/DocumentPaths.cpp
    src/data/FolioRepository.cpp
    src/data/FolioStore.cpp
    src/pdf/ColorRemapper.cpp
    src/pdf/DocumentMagic.cpp
    src/pdf/DocumentFont.cpp
    src/pdf/MuPdfEngine.cpp
    src/pdf/PdfComposer.cpp
    src/pdf/pc_mupdf.c
    src/pdf/TextFinder.cpp
)
target_include_directories(papercut-core PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(papercut-core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql mupdf::mupdf)
if(TARGET mupdf_build)
    add_dependencies(papercut-core mupdf_build)
endif()
if(WIN32)
    target_link_libraries(papercut-core PUBLIC gdi32 comdlg32 user32 advapi32 shell32)
elseif(NOT APPLE)
    target_link_libraries(papercut-core PUBLIC m pthread)
    find_package(Fontconfig QUIET)
    if(Fontconfig_FOUND)
        target_link_libraries(papercut-core PRIVATE Fontconfig::Fontconfig)
        target_compile_definitions(papercut-core PRIVATE PAPERCUT_HAS_FONTCONFIG)
    endif()
endif()

set(PAPERCUT_WIN_RC)
if(WIN32)
    configure_file("${CMAKE_SOURCE_DIR}/assets/papercut-dark-off.ico"
                   "${CMAKE_BINARY_DIR}/papercut.ico" COPYONLY)
    set(PAPERCUT_ICO "papercut.ico")
    set(PAPERCUT_VERSION_STR "${PROJECT_VERSION}")
    if("${PROJECT_VERSION_PATCH}" STREQUAL "")
        set(PAPERCUT_VERSION_PATCH 0)
    else()
        set(PAPERCUT_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    endif()
    set(PAPERCUT_FILEVERSION "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PAPERCUT_VERSION_PATCH},0")
    configure_file("${CMAKE_SOURCE_DIR}/assets/papercut.rc.in"
                   "${CMAKE_BINARY_DIR}/papercut.rc" @ONLY)
    set(PAPERCUT_WIN_RC "${CMAKE_BINARY_DIR}/papercut.rc")
    set_source_files_properties("${PAPERCUT_WIN_RC}" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/papercut-dark-off.ico")
    # RC is built in a MOC-free target so windres is not passed autogen -I paths (breaks when the
    # source tree path contains spaces).
    add_library(papercut-winrc OBJECT "${PAPERCUT_WIN_RC}")
    set_target_properties(papercut-winrc PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
endif()

if(WIN32)
    add_executable(papercut-launch WIN32 src/packaging/win-launch.c)
    target_link_libraries(papercut-launch PRIVATE papercut-winrc user32 shell32)
endif()

add_executable(papercut-tests tests/test_core.cpp)
target_link_libraries(papercut-tests PRIVATE papercut-core)

add_executable(papercut-engine-tests tests/test_engine.cpp)
target_link_libraries(papercut-engine-tests PRIVATE papercut-core)

set(PAPERCUT_RELEASE_OS Linux)
if(WIN32)
    set(PAPERCUT_RELEASE_OS Windows)
elseif(APPLE)
    set(PAPERCUT_RELEASE_OS macOS)
endif()
set(PAPERCUT_RELEASE_DIR "${CMAKE_SOURCE_DIR}/Release/${PAPERCUT_RELEASE_OS}")

if(PAPERCUT_BUILD_GUI)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Qml Network)
    add_library(papercut-gui STATIC
        src/ui/PdfStripItem.cpp
        src/app/AppServices.cpp
        src/app/EngineWorker.cpp
        src/app/DocumentController.cpp
        src/app/ViewerController.cpp
        src/app/FolioController.cpp
        src/app/CombineController.cpp
        src/app/SingleInstance.cpp)
    target_link_libraries(papercut-gui PUBLIC papercut-core Qt6::Quick Qt6::Qml Qt6::QuickControls2 Qt6::Network)

    qt_add_executable(papercut WIN32 src/app/main.cpp src/app/WinWindowChrome.cpp)
    target_link_libraries(papercut PRIVATE papercut-winrc)
    qt_add_resources(papercut qml
        PREFIX /qml
        BASE qml
        FILES
            qml/Main.qml
            qml/AppText.qml
            qml/Icon.qml
            qml/ChromeIcon.qml
            qml/ModeTopBar.qml
            qml/TitleBar.qml
            qml/PageSlider.qml
            qml/PapercutDialog.qml
            qml/PapercutOverlay.qml
            qml/AppLogo.qml
            qml/PapercutSwitch.qml
            qml/ThinGreySlider.qml
            qml/FilterChip.qml
            qml/PapercutField.qml
            qml/ReorderableList.qml
            qml/PreviewOverlay.qml
            qml/DrawerItem.qml
            qml/BookmarkOverlay.qml
            qml/RearrangeOverlay.qml
            qml/FolioJumpOverlay.qml
            qml/ManageFolioOverlay.qml
            qml/CombineOverlay.qml
            qml/AboutOverlay.qml
            qml/CollapsibleSection.qml
            qml/SettingsSwitchRow.qml
            qml/SettingsTapRow.qml
            qml/ColourSettingRow.qml
            qml/ColorPickerDialog.qml
            qml/ManageDocumentsOverlay.qml
            qml/StoredFilesOverlay.qml
            qml/SettingsOverlay.qml
            qml/ShortcutsOverlay.qml
            qml/IconPaths.js)
    qt_add_resources(papercut appicons
        PREFIX /icons
        BASE assets
        FILES
            assets/papercut-dark.ico
            assets/papercut-light.ico
            assets/papercut-dark-off.ico
            assets/papercut-light-off.ico)
    qt_add_resources(papercut uifonts
        PREFIX /fonts
        BASE assets/fonts
        FILES
            assets/fonts/Roboto-Regular.ttf
            assets/fonts/MaterialSymbolsOutlined.ttf)
    target_link_libraries(papercut PRIVATE papercut-gui)
    if(WIN32)
        target_link_libraries(papercut PRIVATE dwmapi)
    elseif(NOT APPLE)
        set_target_properties(papercut PROPERTIES
            INSTALL_RPATH "$ORIGIN/lib"
            BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
    install(TARGETS papercut RUNTIME DESTINATION bin)
endif()

if(PAPERCUT_PACKAGE_RELEASE AND PAPERCUT_BUILD_GUI)
    find_program(PAPERCUT_WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    get_filename_component(PAPERCUT_QT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
    set(_pkg_depends papercut)
    if(WIN32)
        list(APPEND _pkg_depends papercut-launch)
    endif()
    add_custom_target(package-release ALL
        COMMAND ${CMAKE_COMMAND}
            "-DPAPERCUT_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DPAPERCUT_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DPAPERCUT_RELEASE_DIR=${PAPERCUT_RELEASE_DIR}"
            "-DPAPERCUT_WINDEPLOYQT=${PAPERCUT_WINDEPLOYQT}"
            "-DPAPERCUT_QT_ROOT=${PAPERCUT_QT_ROOT}"
            -P "${CMAKE_SOURCE_DIR}/cmake/PackageRelease.cmake"
        DEPENDS ${_pkg_depends}
        COMMENT "Packaging ready-to-run files into Release/${PAPERCUT_RELEASE_OS}"
        VERBATIM)
endif()

message(STATUS "Papercut source : ${CMAKE_SOURCE_DIR}")
message(STATUS "Temp/build dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "Release dir     : ${PAPERCUT_RELEASE_DIR}")

enable_testing()
add_test(NAME papercut-core COMMAND papercut-tests)
add_test(NAME papercut-engine COMMAND papercut-engine-tests)
