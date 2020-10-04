set(eglimage_SOURCES
    eglimageclientbuffer.cpp
    eglimageclientbufferintegration.cpp
    main.cpp
)

ecm_qt_declare_logging_category(eglimage_SOURCES
    HEADER logging.h
    IDENTIFIER KWS_EGLIMAGE
    CATEGORY_NAME kwayland-server.eglimage
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KWayland Server EGL Image Buffer Integration"
    EXPORT KWAYLAND
)

add_library(KWaylandServerEglImageClientBufferPlugin MODULE ${eglimage_SOURCES})
target_link_libraries(KWaylandServerEglImageClientBufferPlugin PRIVATE
    KWaylandServer
    epoxy::epoxy
)
target_compile_definitions(KWaylandServerEglImageClientBufferPlugin PRIVATE
    MESA_EGL_NO_X11_HEADERS
    EGL_NO_X11
    EGL_NO_PLATFORM_SPECIFIC_TYPES
)

install(
    TARGETS
        KWaylandServerEglImageClientBufferPlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwaylandserver/buffer-integration/
)
