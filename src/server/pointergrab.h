/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{

enum class PointerAxisSource;

class SeatInterface;
class SurfaceInterface;

/**
 * The PointerGrab class is the base class for pointer grabs.
 */
class KWAYLANDSERVER_EXPORT PointerGrab : public QObject
{
    Q_OBJECT

public:
    explicit PointerGrab(SeatInterface *seat, QObject *parent = nullptr);

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel() = 0;

    virtual void activate();
    virtual void deactivate();

    virtual void handleFocusChange(SurfaceInterface *surface, const QPointF &position, quint32 serial) = 0;
    virtual void handlePressed(quint32 button, quint32 serial) = 0;
    virtual void handleReleased(quint32 button, quint32 serial) = 0;
    virtual void handleAxis(Qt::Orientation orientation, qreal delta, qint32 discreteDelta, PointerAxisSource source) = 0;
    virtual void handleMotion(const QPointF &position) = 0;
    virtual void handleFrame() = 0;

private:
    SeatInterface *m_seat;
};

} // namespace KWaylandServer
