set(eglstream_SOURCES
    eglstreamclientbuffer.cpp
    eglstreamcontroller.cpp
    eglstreamclientbufferintegration.cpp
    main.cpp
)

ecm_qt_declare_logging_category(eglstream_SOURCES
    HEADER logging.h
    IDENTIFIER KWS_EGLSTREAM
    CATEGORY_NAME kwayland-server.eglstream
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KWayland Server EGL Streams Buffer Integration"
    EXPORT KWAYLAND
)

ecm_add_qtwayland_server_protocol_kde(eglstream_SOURCES
    PROTOCOL ${PLASMA_WAYLAND_PROTOCOLS_DIR}/wayland-eglstream-controller.xml
    BASENAME wl-eglstream-controller
)

add_library(KWaylandServerEglStreamClientBufferPlugin MODULE ${eglstream_SOURCES})
target_link_libraries(KWaylandServerEglStreamClientBufferPlugin PRIVATE
    KWaylandServer
    epoxy::epoxy
)
target_compile_definitions(KWaylandServerEglStreamClientBufferPlugin PRIVATE
    MESA_EGL_NO_X11_HEADERS
    EGL_NO_X11
    EGL_NO_PLATFORM_SPECIFIC_TYPES
)

install(
    TARGETS
        KWaylandServerEglStreamClientBufferPlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwaylandserver/buffer-integration/
)
