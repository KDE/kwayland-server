set(sharedmemory_SOURCES
    sharedmemoryclientbuffer.cpp
    sharedmemoryclientbufferintegration.cpp
    main.cpp
)

ecm_qt_declare_logging_category(sharedmemory_SOURCES
    HEADER logging.h
    IDENTIFIER KWS_SHM
    CATEGORY_NAME kwayland-server.shm
    DEFAULT_SEVERITY Warning
    DESCRIPTION "KWayland Server Shared Memory Buffer Integration"
    EXPORT KWAYLAND
)

add_library(KWaylandServerLinuxSharedMemoryClientBufferPlugin MODULE ${sharedmemory_SOURCES})
target_link_libraries(KWaylandServerLinuxSharedMemoryClientBufferPlugin PRIVATE
    KWaylandServer
    epoxy::epoxy
)
target_compile_definitions(KWaylandServerLinuxSharedMemoryClientBufferPlugin PRIVATE
    MESA_EGL_NO_X11_HEADERS
    EGL_NO_X11
    EGL_NO_PLATFORM_SPECIFIC_TYPES
)

install(
    TARGETS
        KWaylandServerLinuxSharedMemoryClientBufferPlugin
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwaylandserver/buffer-integration/
)
