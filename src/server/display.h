/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2018 David Edmundson <davidedmundson@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_DISPLAY_H
#define WAYLAND_SERVER_DISPLAY_H

#include <QList>
#include <QObject>

#include <KWaylandServer/kwaylandserver_export.h>

#include "clientconnection.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;

namespace KWaylandServer
{
/**
 * @short KWayland Server.
 *
 * This namespace groups all classes related to the Server module.
 *
 * The main entry point into the KWaylandServer API is the Display class.
 * It allows to create a Wayland server and create various global objects on it.
 *
 * KWaylandServer is an API to easily create a head-less Wayland server with a
 * Qt style API.
 *
 * @see Display
 **/

class CompositorInterface;
class DataDeviceManagerInterface;
class DpmsManagerInterface;
class IdleInterface;
class IdleInhibitManagerV1Interface;
class FakeInputInterface;
class OutputInterface;
class OutputDeviceInterface;
class OutputConfigurationInterface;
class OutputManagementInterface;
class PlasmaShellInterface;
class PlasmaWindowManagementInterface;
class QtSurfaceExtensionInterface;
class SeatInterface;
class ShadowManagerInterface;
class BlurManagerInterface;
class ContrastManagerInterface;
class ServerSideDecorationManagerInterface;
class SlideManagerInterface;
class SubCompositorInterface;
class TextInputManagerV2Interface;
class TextInputManagerV3Interface;
class XdgShellInterface;
class RelativePointerManagerV1Interface;
class PointerGesturesV1Interface;
class PointerConstraintsV1Interface;
class XdgForeignV2Interface;
class AppMenuManagerInterface;
class ServerSideDecorationPaletteManagerInterface;
class PlasmaVirtualDesktopManagementInterface;
class XdgOutputManagerV1Interface;
class XdgDecorationManagerV1Interface;
class EglStreamControllerInterface;
class KeyStateInterface;
class LinuxDmabufUnstableV1Interface;
class TabletManagerV2Interface;
class DataControlDeviceManagerV1Interface;
class PrimarySelectionDeviceManagerV1Interface;
class KeyboardShortcutsInhibitManagerV1Interface;
class ViewporterInterface;
class ScreencastV1Interface;
class InputMethodV1Interface;
class InputPanelV1Interface;
class LayerShellV1Interface;

/**
 * @brief Class holding the Wayland server display loop.
 *
 * @todo Improve documentation
 **/
class KWAYLANDSERVER_EXPORT Display : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString socketName READ socketName WRITE setSocketName NOTIFY socketNameChanged)
    Q_PROPERTY(bool automaticSocketNaming READ automaticSocketNaming WRITE setAutomaticSocketNaming NOTIFY automaticSocketNamingChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit Display(QObject *parent = nullptr);
    virtual ~Display();

    /**
     * Sets the basename of the socket to @p name. If @p name is empty, it will use
     * wl_display_add_socket_auto to get a free socket with a filename "wayland-%d".
     **/
    void setSocketName(const QString &name);
    QString socketName() const;

    /**
     * If automaticSocketNaming is true, the manually set socketName is ignored
     * and it will use wl_display_add_socket_auto on start to get a free socket with
     * a filename "wayland-%d" instead. The effective socket is written into socketName.
     * @since 5.55
     **/
    void setAutomaticSocketNaming(bool automaticSocketNaming);
    bool automaticSocketNaming() const;

    quint32 serial();
    quint32 nextSerial();

    /**
     * How to setup the server connection.
     * @li ConnectToSocket: the server will open the socket identified by the socket name
     * @li ConnectClientsOnly: only connections through createClient are possible
     **/
    enum class StartMode {
        ConnectToSocket,
        ConnectClientsOnly
    };
    /**
     * Start accepting client connections. If the display has started successfully, this
     * function returns @c true; otherwise @c false is returned.
     */
    bool start(StartMode mode = StartMode::ConnectToSocket);
    /**
     * Dispatches pending events in a blocking way. May only be used if the Display is
     * created and started before the QCoreApplication is created. Once the QCoreApplication
     * is created and the event loop is started this method delegates to the normal dispatch
     * handling.
     * @see startLoop
     **/
    void dispatchEvents(int msecTimeout = -1);

    /**
     * Create a client for the given file descriptor.
     *
     * The client is created as if it connected through the normal server
     * socket. This method can be used to create a connection bypassing the
     * normal socket connection. It's recommended to use together with
     * socketpair and pass the other side of the socket to the client.
     *
     * @param fd The file descriptor for the socket to the client
     * @returns The new ClientConnection or @c null on failure.
     **/
    ClientConnection *createClient(int fd);

    operator wl_display*();
    operator wl_display*() const;
    bool isRunning() const;

    OutputInterface *createOutput(QObject *parent = nullptr);
    void removeOutput(OutputInterface *output);
    QList<OutputInterface*> outputs() const;

    OutputDeviceInterface *createOutputDevice(QObject *parent = nullptr);
    void removeOutputDevice(OutputDeviceInterface *output);
    QList<OutputDeviceInterface*> outputDevices() const;

    CompositorInterface *createCompositor(QObject *parent = nullptr);
    void createShm();
    SeatInterface *createSeat(QObject *parent = nullptr);
    /**
     * @returns All SeatInterface currently managed on the Display.
     * @since 5.6
     **/
    QVector<SeatInterface*> seats() const;
    SubCompositorInterface *createSubCompositor(QObject *parent = nullptr);
    DataDeviceManagerInterface *createDataDeviceManager(QObject *parent = nullptr);
    OutputManagementInterface *createOutputManagement(QObject *parent = nullptr);
    PlasmaShellInterface *createPlasmaShell(QObject *parent = nullptr);
    PlasmaWindowManagementInterface *createPlasmaWindowManagement(QObject *parent = nullptr);
    IdleInterface *createIdle(QObject *parent = nullptr);
    FakeInputInterface *createFakeInput(QObject *parent = nullptr);
    ShadowManagerInterface *createShadowManager(QObject *parent = nullptr);
    BlurManagerInterface *createBlurManager(QObject *parent = nullptr);
    ContrastManagerInterface *createContrastManager(QObject *parent = nullptr);
    SlideManagerInterface *createSlideManager(QObject *parent = nullptr);
    DpmsManagerInterface *createDpmsManager(QObject *parent = nullptr);

    /** @since 5.60 */
    KeyStateInterface *createKeyStateInterface(QObject *parent = nullptr);

    /**
     * @since 5.6
     **/
    ServerSideDecorationManagerInterface *createServerSideDecorationManager(QObject *parent = nullptr);
    /**
     * Create the text input manager in interface @p version.
     * @returns The created manager object
     * @since 5.23
     **/
    TextInputManagerV2Interface *createTextInputManagerV2(QObject *parent = nullptr);

    /**
     * Create a text input manager v3
     * @returns The created manager object
     * @since 5.21
     */
    TextInputManagerV3Interface *createTextInputManagerV3(QObject *parent = nullptr);

    /**
     * Creates the XdgShell in interface @p version.
     *
     * @since 5.25
     **/
    XdgShellInterface *createXdgShell(QObject *parent = nullptr);

    /**
     * Creates the RelativePointerManagerV1Interface
     *
     * @returns The created manager object
     * @since 5.28
     **/
    RelativePointerManagerV1Interface *createRelativePointerManagerV1(QObject *parent = nullptr);

    /**
     * Creates the PointerGesturesV1Interface
     *
     * @returns The created manager object
     * @since 5.29
     **/
    PointerGesturesV1Interface *createPointerGesturesV1(QObject *parent = nullptr);

    /**
     * Creates the PointerConstraintsV1Interface
     *
     * @returns The created manager object
     * @since 5.29
     **/
    PointerConstraintsV1Interface *createPointerConstraintsV1(QObject *parent = nullptr);

    /**
     * Creates the XdgForeignV2Interface in interface @p version
     *
     * @returns The created manager object
     * @since 5.40
     **/
    XdgForeignV2Interface *createXdgForeignV2Interface(QObject *parent = nullptr);

    /**
     * Creates the IdleInhibitManagerInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 5.41
     **/
    IdleInhibitManagerV1Interface *createIdleInhibitManagerV1(QObject *parent = nullptr);

    /**
     * Creates the AppMenuManagerInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 5.42
     **/
    AppMenuManagerInterface *createAppMenuManagerInterface(QObject *parent = nullptr);

    /**
     * Creates the ServerSideDecorationPaletteManagerInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 5.42
     **/
    ServerSideDecorationPaletteManagerInterface *createServerSideDecorationPaletteManager(QObject *parent = nullptr);

    /**
     * Creates the LinuxDmabufUnstableV1Interface in interface @p version.
     *
     * @returns A pointer to the created interface
     **/
    LinuxDmabufUnstableV1Interface *createLinuxDmabufInterface(QObject *parent = nullptr);

    /**
     * Creates the XdgOutputManagerInterface
     *
     * @return the created manager
     * @since 5.47
     */
    XdgOutputManagerV1Interface *createXdgOutputManagerV1(QObject *parent = nullptr);


    /**
     * Creates the PlasmaVirtualDesktopManagementInterface in interface @p version.
     *
     * @returns The created manager object
     * @since 5.52
     **/
    PlasmaVirtualDesktopManagementInterface *createPlasmaVirtualDesktopManagement(QObject *parent = nullptr);

    /**
     * Creates the XdgDecorationManagerInterface
     * @arg shellInterface A created XdgShellInterface based on XDG_WM_BASE
     *
     * @return the created manager
     * @since 5.54
     */
    XdgDecorationManagerV1Interface *createXdgDecorationManagerV1(QObject *parent = nullptr);

    /**
     * Creates the EglStreamControllerInterface
     *
     * @return the created EGL Stream controller
     * @since 5.58
     */
    EglStreamControllerInterface *createEglStreamControllerInterface(QObject *parent = nullptr);

    InputMethodV1Interface *createInputMethodInterface(QObject *parent = nullptr);
    InputPanelV1Interface *createInputPanelInterface(QObject *parent = nullptr);

    /**
     * Creates the DataControlDeviceManagerV1
     *
     */
    DataControlDeviceManagerV1Interface *createDataControlDeviceManagerV1(QObject *parent = nullptr);

    /**
     * Creates the entry point to support wacom-like tablets and pens.
     *
     * @since 5.67
     */
    TabletManagerV2Interface *createTabletManagerV2(QObject *parent = nullptr);

    /**
     * Creates the KeyboardShortcutsInhibitorV1Interface
     */
    KeyboardShortcutsInhibitManagerV1Interface *createKeyboardShortcutsInhibitManagerV1(QObject *parent = nullptr);

    /**
     * Creates the viewporter compositor extension.
     */
    ViewporterInterface *createViewporter(QObject *parent = nullptr);

    /**
     * Creates the PrimarySelectionDeviceManagerV1Interface
     */
    PrimarySelectionDeviceManagerV1Interface *createPrimarySelectionDeviceManagerV1(QObject *parent = nullptr);

    /**
     * Creates an interface to request video feeds of different compositor resources
     */
    ScreencastV1Interface *createScreencastV1Interface(QObject *parent = nullptr);

    /**
     * Creates the layer shell compositor extension.
     */
    LayerShellV1Interface *createLayerShellV1(QObject *parent = nullptr);

    /**
     * Gets the ClientConnection for the given @p client.
     * If there is no ClientConnection yet for the given @p client, it will be created.
     * @param client The native client for which the ClientConnection is retrieved
     * @return The ClientConnection for the given native client
     **/
    ClientConnection *getConnection(wl_client *client);
    QVector<ClientConnection*> connections() const;

    /**
     * Set the EGL @p display for this Wayland display.
     * The EGLDisplay can only be set once and must be alive as long as the Wayland display
     * is alive. The user should have set up the binding between the EGLDisplay and the
     * Wayland display prior to calling this method.
     *
     * @see eglDisplay
     * @since 5.3
     **/
    void setEglDisplay(void *display);
    /**
     * @returns the EGLDisplay used for this Wayland display or EGL_NO_DISPLAY if not set.
     * @see setEglDisplay
     * @since 5.3
     **/
    void *eglDisplay() const;

Q_SIGNALS:
    void socketNameChanged(const QString&);
    void automaticSocketNamingChanged(bool);
    void runningChanged(bool);
    void aboutToTerminate();
    void clientConnected(KWaylandServer::ClientConnection*);
    void clientDisconnected(KWaylandServer::ClientConnection*);

private:
    class Private;
    QScopedPointer<Private> d;
};

}

#endif
