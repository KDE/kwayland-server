set(linuxdmabufv1_SOURCES
    linuxdmabufv1clientbuffer.cpp
    linuxdmabufv1clientbufferintegration.cpp
    linuxdmabufv1interface.cpp
    main.cpp
)

ecm_qt_declare_logging_category(linuxdmabufv1_SOURCES
    HEADER logging.h
    IDENTIFIER KWS_LINUXDMABUF
    CATEGORY_NAME kwayland-server.linuxdmabufv1
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KWayland Server Linux DMA-BUF V1 Buffer Integration"
    EXPORT KWAYLAND
)

ecm_add_qtwayland_server_protocol_kde(linuxdmabufv1_SOURCES
    PROTOCOL ${WaylandProtocols_DATADIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
    BASENAME linux-dmabuf-unstable-v1
)

ecm_add_qtwayland_server_protocol_kde(linuxdmabufv1_SOURCES
    PROTOCOL ${Wayland_DATADIR}/wayland.xml
    BASENAME wayland
)

add_library(KWaylandServerLinuxDmaBufV1ClientBufferPlugin MODULE ${linuxdmabufv1_SOURCES})
target_link_libraries(KWaylandServerLinuxDmaBufV1ClientBufferPlugin PRIVATE
    KWaylandServer
    epoxy::epoxy
)
target_compile_definitions(KWaylandServerLinuxDmaBufV1ClientBufferPlugin PRIVATE
    MESA_EGL_NO_X11_HEADERS
    EGL_NO_X11
    EGL_NO_PLATFORM_SPECIFIC_TYPES
)

install(
    TARGETS
        KWaylandServerLinuxDmaBufV1ClientBufferPlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwaylandserver/buffer-integration/
)
