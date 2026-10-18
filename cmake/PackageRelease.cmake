# Stage launcher, bin/, and README.md under Source/Release/{Windows,Linux}.

if(NOT PAPERCUT_BUILD_DIR OR NOT PAPERCUT_SOURCE_DIR OR NOT PAPERCUT_RELEASE_DIR)
    message(FATAL_ERROR "PAPERCUT_BUILD_DIR, PAPERCUT_SOURCE_DIR, and PAPERCUT_RELEASE_DIR are required")
endif()

function(papercut_find_built name out)
    foreach(_c "" "/Release" "/RelWithDebInfo" "/MinSizeRel" "/Debug")
        if(WIN32)
            set(_p "${PAPERCUT_BUILD_DIR}${_c}/${name}.exe")
        else()
            set(_p "${PAPERCUT_BUILD_DIR}${_c}/${name}")
        endif()
        if(EXISTS "${_p}")
            set(${out} "${_p}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "Could not find built ${name} under ${PAPERCUT_BUILD_DIR}")
endfunction()

file(REMOVE_RECURSE "${PAPERCUT_RELEASE_DIR}")
file(MAKE_DIRECTORY "${PAPERCUT_RELEASE_DIR}/bin")

if(EXISTS "${PAPERCUT_SOURCE_DIR}/README.md")
    file(COPY "${PAPERCUT_SOURCE_DIR}/README.md" DESTINATION "${PAPERCUT_RELEASE_DIR}")
endif()
foreach(_lic Roboto-LICENSE.txt MaterialSymbols-LICENSE.txt)
    if(EXISTS "${PAPERCUT_SOURCE_DIR}/assets/fonts/${_lic}")
        file(COPY "${PAPERCUT_SOURCE_DIR}/assets/fonts/${_lic}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin")
    endif()
endforeach()

if(WIN32)
    papercut_find_built(papercut _app)
    papercut_find_built(papercut-launch _launch)
    file(COPY "${_app}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin")
    if(NOT PAPERCUT_WINDEPLOYQT)
        message(FATAL_ERROR "PAPERCUT_WINDEPLOYQT is required on Windows")
    endif()
    execute_process(
        COMMAND "${PAPERCUT_WINDEPLOYQT}"
            --release
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            --qmldir "${PAPERCUT_SOURCE_DIR}/qml"
            --compiler-runtime
            "${PAPERCUT_RELEASE_DIR}/bin/papercut.exe"
        RESULT_VARIABLE _rv)
    if(NOT _rv EQUAL 0)
        message(FATAL_ERROR "windeployqt failed (${_rv})")
    endif()
    file(COPY "${_launch}" DESTINATION "${PAPERCUT_RELEASE_DIR}")
    file(RENAME "${PAPERCUT_RELEASE_DIR}/papercut-launch.exe" "${PAPERCUT_RELEASE_DIR}/papercut.exe")
else()
    papercut_find_built(papercut _app)
    file(COPY "${_app}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin")
    file(WRITE "${PAPERCUT_RELEASE_DIR}/papercut"
        "#!/bin/sh\nDIR=$(CDPATH= cd -- \"$(dirname \"$0\")\" && pwd)\nexec \"$DIR/bin/papercut\" \"$@\"\n")
    file(CHMOD "${PAPERCUT_RELEASE_DIR}/papercut" PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    if(PAPERCUT_QT_ROOT AND EXISTS "${PAPERCUT_QT_ROOT}/lib")
        file(MAKE_DIRECTORY "${PAPERCUT_RELEASE_DIR}/bin/lib")
        file(GLOB _qt_sos
            "${PAPERCUT_QT_ROOT}/lib/libQt6*.so*"
            "${PAPERCUT_QT_ROOT}/lib/libicu*.so*")
        foreach(_so ${_qt_sos})
            file(COPY "${_so}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin/lib")
        endforeach()
        foreach(_plug platforms imageformats sqldrivers tls networkinformation xcbglintegrations generic iconengines)
            if(EXISTS "${PAPERCUT_QT_ROOT}/plugins/${_plug}")
                file(COPY "${PAPERCUT_QT_ROOT}/plugins/${_plug}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin/plugins")
            endif()
        endforeach()
        foreach(_qml QtQuick QtQml QML Qt)
            if(EXISTS "${PAPERCUT_QT_ROOT}/qml/${_qml}")
                file(COPY "${PAPERCUT_QT_ROOT}/qml/${_qml}" DESTINATION "${PAPERCUT_RELEASE_DIR}/bin/qml")
            endif()
        endforeach()
        file(WRITE "${PAPERCUT_RELEASE_DIR}/bin/qt.conf"
            "[Paths]\nPrefix = .\nPlugins = plugins\nQmlImports = qml\nLibraries = lib\n")
    endif()
endif()

message(STATUS "Packaged ${PAPERCUT_RELEASE_DIR}")
