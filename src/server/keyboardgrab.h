/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <KWaylandServer/kwaylandserver_export.h>

#include <QObject>

namespace KWaylandServer
{

class SeatInterface;
class SurfaceInterface;

/**
 * The KeyboardGrab class is the base class for keyboard grabs.
 */
class KWAYLANDSERVER_EXPORT KeyboardGrab : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardGrab(SeatInterface *seat, QObject *parent = nullptr);

    bool isActive() const;
    void setActive(bool active);

    SeatInterface *seat() const;
    virtual void cancel() = 0;

    virtual void activate();
    virtual void deactivate();

    virtual void handleFocusChange(SurfaceInterface *surface, quint32 serial) = 0;
    virtual void handlePressEvent(quint32 keyCode) = 0;
    virtual void handleReleaseEvent(quint32 keyCode) = 0;
    virtual void handleModifiers(quint32 depressed, quint32 latched, quint32 locked, quint32 group) = 0;

private:
    SeatInterface *m_seat;
};

} // namespace KWaylandServer
