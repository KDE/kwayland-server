/*
    SPDX-FileCopyrightText: 2017 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// Qt
#include <QtTest>
// KWin
#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/region.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/surface.h"
#include "KWayland/Client/xdgdbusannotation_v1.h"
#include "KWayland/Client/xdgshell.h"
// #include "KWayland/Client/xdgdbusannotation_v1.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/xdgdbusannotation_v1_interface.h"
#include "../../src/server/xdgshell_interface.h"

using namespace KWayland;
using namespace KWaylandServer;

Q_DECLARE_METATYPE(KWaylandServer::XdgDBusAnnotationV1Interface::InterfaceAddress)

static const QString s_socketName = QStringLiteral("kwayland-test-xdg_dbus_annotation-0");

class TestAnnotation : public QObject
{
    Q_OBJECT
public:
    explicit TestAnnotation(QObject *parent = nullptr);
private Q_SLOTS:
    void init();
    void cleanup();

    /// tests to make sure that we can locate client-bound annotations by client
    void testPerClient();

    /// tests to make sure that we can locate toplevel-bound annotations by toplevel
    void testPerToplevel();

    /// tests to make sure update machinery is working
    void testUpdateAnnotation();

private:
    struct SurfaceData {
        QSharedPointer<Client::Surface> cliS;
        QSharedPointer<Client::XdgShellSurface> cliX;

        QSharedPointer<SurfaceInterface> servS;
        QSharedPointer<XdgToplevelInterface> servX;
    };

    // yes, this does indeed need this much boilerplate.
    SurfaceData newXdgSurface()
    {
        QSignalSpy surfaceCreatedSpy(m_compositorInterface, &CompositorInterface::surfaceCreated);
        Q_ASSERT(surfaceCreatedSpy.isValid());
        QSignalSpy xdgSurfaceCreatedSpy(m_xdgShellInterface, &XdgShellInterface::toplevelCreated);
        Q_ASSERT(xdgSurfaceCreatedSpy.isValid());

        // create surface
        QSharedPointer<Client::Surface> surface(m_compositor->createSurface());
        Q_ASSERT(!surface.isNull());
        Q_ASSERT(surfaceCreatedSpy.wait());
        auto serverSurface = surfaceCreatedSpy.first().first().value<SurfaceInterface *>();
        Q_ASSERT(serverSurface);

        // create shell surface
        QSharedPointer<Client::XdgShellSurface> xdgSurface(m_xdgShell->createSurface(surface.data()));
        Q_ASSERT(!xdgSurface.isNull());
        Q_ASSERT(xdgSurfaceCreatedSpy.wait());
        auto serverXdgSurfacce = xdgSurfaceCreatedSpy.first().first().value<XdgToplevelInterface *>();
        Q_ASSERT(serverXdgSurfacce);

        return SurfaceData{
            surface,
            xdgSurface,

            QSharedPointer<SurfaceInterface>(serverSurface),
            QSharedPointer<XdgToplevelInterface>(serverXdgSurfacce),
        };
    }

    // server stuff
    Display *m_display = nullptr;
    CompositorInterface *m_compositorInterface = nullptr;
    XdgDBusAnnotationManagerV1Interface *m_annotationManagerInterface = nullptr;
    XdgShellInterface *m_xdgShellInterface = nullptr;

    // client
    Client::ConnectionThread *m_connection = nullptr;
    Client::Compositor *m_compositor = nullptr;
    Client::EventQueue *m_queue = nullptr;
    Client::XdgShell *m_xdgShell = nullptr;
    Client::XdgDBusAnnotationManagerV1 *m_annotationManager = nullptr;

    QThread *m_thread = nullptr;
};

TestAnnotation::TestAnnotation(QObject *parent)
    : QObject(parent)
    , m_display(nullptr)
    , m_compositorInterface(nullptr)
    , m_connection(nullptr)
    , m_compositor(nullptr)
    , m_queue(nullptr)
    , m_thread(nullptr)
{
}

void TestAnnotation::init()
{
    // set up the display...
    m_display = new Display(this);
    m_display->addSocketName(s_socketName);
    m_display->start();
    QVERIFY(m_display->isRunning());

    // now compositor
    m_compositorInterface = new CompositorInterface(m_display, m_display);

    // now xdg shell
    m_xdgShellInterface = new XdgShellInterface(m_display, m_display);

    // now the annotation manager
    m_annotationManagerInterface = new XdgDBusAnnotationManagerV1Interface(m_display, m_display);

    // now let's set up the client...
    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &Client::ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    // hope we're working
    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    // event queue time
    m_queue = new Client::EventQueue(this);
    m_queue->setup(m_connection);

    // registry fun
    Client::Registry registry;
    QSignalSpy interfacesAnnouncedSpy(&registry, &Client::Registry::interfacesAnnounced);
    QVERIFY(interfacesAnnouncedSpy.isValid());
    QSignalSpy interfaceAnnouncedSpy(&registry, &Client::Registry::interfaceAnnounced);
    QVERIFY(interfaceAnnouncedSpy.isValid());
    QSignalSpy outputAnnouncedSpy(&registry, &Client::Registry::outputAnnounced);
    QVERIFY(outputAnnouncedSpy.isValid());

    QSignalSpy xdgShellAnnouncedSpy(&registry, &Client::Registry::xdgShellStableAnnounced);
    QVERIFY(xdgShellAnnouncedSpy.isValid());
    registry.setEventQueue(m_queue);
    registry.create(m_connection);
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(interfacesAnnouncedSpy.wait());

    // now we fill in the client stuff
    m_compositor = registry.createCompositor(registry.interface(Client::Registry::Interface::Compositor).name,
                                             registry.interface(Client::Registry::Interface::Compositor).version,
                                             this);
    QVERIFY(m_compositor);
    QVERIFY(m_compositor->isValid());

    m_xdgShell = registry.createXdgShell(registry.interface(Client::Registry::Interface::XdgShellStable).name,
                                         registry.interface(Client::Registry::Interface::XdgShellStable).version,
                                         this);
    QVERIFY(m_xdgShell);
    QVERIFY(m_xdgShell->isValid());

    m_annotationManager = registry.createXdgDBusAnnotationManagerV1(registry.interface(Client::Registry::Interface::XdgDBusAnnotationManagerV1).name,
                                                                  registry.interface(Client::Registry::Interface::XdgDBusAnnotationManagerV1).version,
                                                                  this);
}

void TestAnnotation::cleanup()
{
    const auto clean = [](auto it) {
        if (*it != nullptr) {
            delete *it;
            *it = nullptr;
        }
    };

    // clean(&m_display);
    // clean(&m_compositorInterface);
    // clean(&m_annotationManagerInterface);
    // clean(&m_xdgShellInterface);
    // clean(&m_connection);
    // clean(&m_compositor);
    // clean(&m_queue);

    if (m_connection) {
        m_connection->deleteLater();
        m_connection = nullptr;
    }

    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
}

void TestAnnotation::testPerClient()
{
    QSignalSpy newAnnotationSpy(m_annotationManagerInterface, &XdgDBusAnnotationManagerV1Interface::annotationCreated);
    QVERIFY(newAnnotationSpy.isValid());

    auto annotation = m_annotationManager->createForClient("geil");
    Q_UNUSED(annotation)

    QVERIFY(newAnnotationSpy.wait());

    auto serverAnnotation = newAnnotationSpy.first().first().value<XdgDBusAnnotationV1Interface*>();

    QVERIFY(serverAnnotation->name() == "geil");
    QVERIFY(std::holds_alternative<ClientConnection*>(serverAnnotation->target()));
}

void TestAnnotation::testPerToplevel()
{
    Q_ASSERT(false);

    auto surface = newXdgSurface();

    QSignalSpy newAnnotationSpy(m_annotationManagerInterface, &XdgDBusAnnotationManagerV1Interface::annotationCreated);
    QVERIFY(newAnnotationSpy.isValid());

    auto annotation = m_annotationManager->createForToplevel(surface.cliX.data(), "geil");
    Q_UNUSED(annotation)

    QVERIFY(newAnnotationSpy.wait());

    auto serverAnnotation = newAnnotationSpy.first().first().value<XdgDBusAnnotationV1Interface*>();

    QVERIFY(serverAnnotation->name() == "geil");
    QVERIFY(std::holds_alternative<XdgToplevelInterface*>(serverAnnotation->target()));
}

void TestAnnotation::testUpdateAnnotation()
{
    QSignalSpy newAnnotationSpy(m_annotationManagerInterface, &XdgDBusAnnotationManagerV1Interface::annotationCreated);
    QVERIFY(newAnnotationSpy.isValid());

    auto annotation = m_annotationManager->createForClient("geil");

    QVERIFY(newAnnotationSpy.wait());

    auto serverAnnotation = newAnnotationSpy.first().first().value<XdgDBusAnnotationV1Interface*>();

    QSignalSpy changeSpy(serverAnnotation, &XdgDBusAnnotationV1Interface::addressChanged);
    QVERIFY(changeSpy.isValid());

    annotation->setAddress("foo", "bar");

    QVERIFY(changeSpy.wait());
}

QTEST_GUILESS_MAIN(TestAnnotation)

#include "test_wayland_annotation.moc"
