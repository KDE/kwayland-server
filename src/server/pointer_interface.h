/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#ifndef WAYLAND_SERVER_POINTER_INTERFACE_H
#define WAYLAND_SERVER_POINTER_INTERFACE_H

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

struct wl_resource;

namespace KWaylandServer
{

class CursorPrivate;
class Cursor;
class PointerInterfacePrivate;
class SeatInterface;
class SurfaceInterface;

enum class PointerAxisSource;

/**
 * The PointerInterface class represents one or more input devices such as mice, which control
 * the pointer location.
 *
 * @see SeatInterface
 */
class KWAYLANDSERVER_EXPORT PointerInterface : public QObject
{
    Q_OBJECT

public:
    explicit PointerInterface(SeatInterface *seat);
    ~PointerInterface() override;

    /**
     * Returns the focused pointer surface. Note that the returned value may be different
     * from SurfaceInterface::focusedSurfacePointerSurface() because this function returns
     * the effective focused surface.
     */
    SurfaceInterface *focusedSurface() const;
    /**
     * Sets the effective focused pointer surface to @a surface. The @a position indicates
     * where the pointer has entered the surface.
     */
    void setFocusedSurface(SurfaceInterface *surface, const QPointF &position, quint32 serial);

    Cursor *cursor() const;
    SeatInterface *seat() const;

    /**
     * @returns The PointerInterface for the @p native resource.
     */
    static PointerInterface *get(wl_resource *native);

    void sendPressed(quint32 button, quint32 serial);
    void sendReleased(quint32 button, quint32 serial);
    void sendAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source);
    void sendMotion(const QPointF &position);
    void sendFrame();

Q_SIGNALS:
    /**
     * Signal emitted whenever the Cursor changes.
     */
    void cursorChanged();

private:
    QScopedPointer<PointerInterfacePrivate> d;
    friend class PointerInterfacePrivate;
};

/**
 * @brief Class encapsulating a Cursor image.
 */
class KWAYLANDSERVER_EXPORT Cursor : public QObject
{
    Q_OBJECT

public:
    virtual ~Cursor();
    /**
     * The hotspot of the cursor image in surface-relative coordinates.
     */
    QPoint hotspot() const;
    /**
     * The entered serial when the Cursor got set.
     */
    quint32 enteredSerial() const;
    /**
     * The PointerInterface this Cursor belongs to.
     */
    PointerInterface *pointer() const;
    /**
     * The SurfaceInterface for the image content of the Cursor.
     */
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void hotspotChanged();
    void enteredSerialChanged();
    void surfaceChanged();
    void changed();

private:
    QScopedPointer<CursorPrivate> d;
    friend class PointerInterfacePrivate;
    explicit Cursor(PointerInterface *parent);
};

} // namespace KWaylandServer

Q_DECLARE_METATYPE(KWaylandServer::PointerInterface*)

#endif
