add_subdirectory(tools)
add_subdirectory(server)

ecm_qt_install_logging_categories(
    EXPORT KWAYLANDSERVER
    FILE kwaylandserver.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

if(BUILD_QCH)
    macro(_make_absolute var_name base_path)
        set(_result)
        foreach(_path ${${var_name}})
            if(IS_ABSOLUTE "${_path}")
                list(APPEND _result "${_path}")
            else()
                list(APPEND _result "${base_path}/${_path}")
            endif()
        endforeach()
        set(${var_name} ${_result})
    endmacro()

    _make_absolute(KWaylandClient_APIDOX_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/client")
    _make_absolute(KWaylandServer_APIDOX_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/server")

    ecm_add_qch(
        KWaylandServer_QCH
        NAME KWaylandServer
        VERSION ${PROJECT_VERSION}
        ORG_DOMAIN org.kde
        SOURCES # using only public headers, to cover only public API
            ${KWaylandServer_APIDOX_SRCS}
        MD_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md"
        LINK_QCHS
            Qt5Gui_QCH
            KF5Wayland_QCH
        INCLUDE_DIRS
            ${KWaylandServer_APIDOX_BUILD_INCLUDE_DIRS}
        BLANK_MACROS
            KWAYLANDSERVER_EXPORT
            KWAYLANDSERVER_DEPRECATED
            KWAYLANDSERVER_DEPRECATED_EXPORT
            "KWAYLANDSERVER_DEPRECATED_VERSION(x, y, t)"
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()
