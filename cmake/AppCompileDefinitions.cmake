# dencla_common_app_compile_definitions(<target> VERSION <version-string>)
#
# Applies this author's standard compile-definition set to a Sailfish app target: APP_VERSION
# from the given version string, QT_QML_DEBUG for Debug/RelWithDebInfo (so QML debugging/
# profiling attaches), and QT_NO_DEBUG_OUTPUT for every config except Debug - qDebug() calls
# would otherwise spam journald on every release install (see harbour-sailreader's
# sailfish/CLAUDE.md entry for the incident this was added for). Each app is still free to add
# its own extra app-specific compile definitions (e.g. a debug-tools-only feature flag)
# alongside this via its own separate target_compile_definitions() call - CMake accumulates
# multiple calls on the same target.
function(dencla_common_app_compile_definitions TARGET)
    cmake_parse_arguments(ARG "" "VERSION" "" ${ARGN})
    target_compile_definitions(${TARGET} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT>
        APP_VERSION="${ARG_VERSION}"
    )
endfunction()
