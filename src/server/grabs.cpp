/*
    SPDX-FileCopyrightText: 2021 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "grabs.h"

namespace KWaylandServer
{

bool PointerGrab::pointerEvent(QMouseEvent*, quint32)
{
    return false;
}

bool PointerGrab::wheelEvent(QWheelEvent*)
{
    return false;
}

bool KeyboardGrab::keyEvent(QKeyEvent*)
{
    return false;
}

bool TouchGrab::touchDown(qint32, const QPointF&, quint32)
{
    return false;
}

bool TouchGrab::touchMotion(qint32, const QPointF&, quint32)
{
    return false;
}

bool TouchGrab::touchUp(qint32, quint32)
{
    return false;
}

} // namespace KWaylandServer
