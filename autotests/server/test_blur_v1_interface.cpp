/*
    SPDX-FileCopyrightText: 2020 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QThread>
#include <QtTest>

#include "../../src/server/blur_v1_interface.h"
#include "../../src/server/compositor_interface.h"
#include "../../src/server/display.h"
#include "../../src/server/surface_interface.h"

#include "KWayland/Client/compositor.h"
#include "KWayland/Client/connection_thread.h"
#include "KWayland/Client/event_queue.h"
#include "KWayland/Client/registry.h"
#include "KWayland/Client/shm_pool.h"
#include "KWayland/Client/surface.h"

#include "qwayland-ext-blur-unstable-v1.h"

using namespace KWaylandServer;

class BlurManagerV1 : public QtWayland::zext_blur_manager_v1
{
};

class BlurV1 : public QtWayland::zext_blur_v1
{
};

class TestBlurV1Interface : public QObject
{
    Q_OBJECT

public:
    ~TestBlurV1Interface() override;

private Q_SLOTS:
    void initTestCase();
    void testCropScale();

private:
    KWayland::Client::ConnectionThread *m_connection;
    KWayland::Client::EventQueue *m_queue;
    KWayland::Client::Compositor *m_clientCompositor;
    KWayland::Client::ShmPool *m_shm;

    QThread *m_thread;
    Display m_display;
    CompositorInterface *m_serverCompositor;
    BlurManagerV1 *m_blurManager;
};

static const QString s_socketName = QStringLiteral("kwin-wayland-server-blur-v1-test-0");

void TestBlurV1Interface::initTestCase()
{
    m_display.setSocketName(s_socketName);
    m_display.start();
    QVERIFY(m_display.isRunning());

    m_display.createShm();
    m_display.createViewporter();

    m_serverCompositor = m_display.createCompositor(this);

    m_connection = new KWayland::Client::ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &KWayland::Client::ConnectionThread::connected);
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());
    QVERIFY(!m_connection->connections().isEmpty());

    m_queue = new KWayland::Client::EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    auto registry = new KWayland::Client::Registry(this);
    connect(registry, &KWayland::Client::Registry::interfaceAnnounced, this, [this, registry](const QByteArray &interface, quint32 id, quint32 version) {
        if (interface == zext_blur_manager_v1_interface.name) {
            m_blurManager = new BlurManagerV1();
            m_blurManager->init(*registry, id, version);
        }
    });
    QSignalSpy allAnnouncedSpy(registry, &KWayland::Client::Registry::interfaceAnnounced);
    QSignalSpy compositorSpy(registry, &KWayland::Client::Registry::compositorAnnounced);
    QSignalSpy shmSpy(registry, &KWayland::Client::Registry::shmAnnounced);
    registry->setEventQueue(m_queue);
    registry->create(m_connection->display());
    QVERIFY(registry->isValid());
    registry->setup();
    QVERIFY(allAnnouncedSpy.wait());

    m_clientCompositor = registry->createCompositor(compositorSpy.first().first().value<quint32>(),
                                                    compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_clientCompositor->isValid());

    m_shm = registry->createShmPool(shmSpy.first().first().value<quint32>(),
                                    shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
}

TestBlurV1Interface::~TestBlurV1Interface()
{
    if (m_blurManager) {
        delete m_blurManager;
        m_blurManager = nullptr;
    }
    if (m_shm) {
        delete m_shm;
        m_shm = nullptr;
    }
    if (m_queue) {
        delete m_queue;
        m_queue = nullptr;
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
    }
    m_connection->deleteLater();
    m_connection = nullptr;
}

void TestBlurV1Interface::testCropScale()
{
    // Create a test surface.
    QSignalSpy serverSurfaceCreatedSpy(m_serverCompositor, &CompositorInterface::surfaceCreated);
    QVERIFY(serverSurfaceCreatedSpy.isValid());
    QScopedPointer<KWayland::Client::Surface> clientSurface(m_clientCompositor->createSurface(this));
    QVERIFY(serverSurfaceCreatedSpy.wait());
    SurfaceInterface *serverSurface = serverSurfaceCreatedSpy.first().first().value<SurfaceInterface *>();
    QVERIFY(serverSurface);

    QSignalSpy serverSurfaceCommittedSpy(serverSurface, &SurfaceInterface::committed);
    QVERIFY(serverSurfaceCommittedSpy.isValid());

    // Map the surface.
    QImage image(QSize(200, 100), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    KWayland::Client::Buffer::Ptr buffer = m_shm->createBuffer(image);
    clientSurface->attachBuffer(buffer);
    clientSurface->setScale(2);
    clientSurface->damage(image.rect());
    clientSurface->commit(KWayland::Client::Surface::CommitFlag::None);
    QVERIFY(serverSurfaceCommittedSpy.wait());
    QCOMPARE(serverSurface->size(), QSize(100, 50));

    // Create a blur for the surface.
    QScopedPointer<BlurV1> clientBlur(new BlurV1);
    clientBlur->init(m_blurManager->get_blur(*clientSurface));

    // TODO
}

QTEST_GUILESS_MAIN(TestBlurV1Interface)

#include "test_blur_v1_interface.moc"
