/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ktilewidget.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

bool KTileWidget::clicksAllowed = true;
QList<QPixmap> *KTileWidget::pixmaps = nullptr;

void KTileWidget::enableClicks(bool flag)
{
    clicksAllowed = flag;
}

void KTileWidget::setPixmaps(QList<QPixmap> *ptr)
{
    pixmaps = ptr;
}

KTileWidget::KTileWidget(QWidget *parent, int row, int col)
    : QFrame(parent)
    , m_row(row)
    , m_col(col)
{
    setMinimumSize(20, 20);
    setFrameStyle(QFrame::Panel | QFrame::Raised);
    int h = height();
    int w = width();
    setLineWidth((h < w ? h : w) / 14);

    reset();
    update();
}

void KTileWidget::reset()
{
    m_owner = Owner::Nobody;
    m_playerHighlight = 0;
    m_effect = Effect::None;
    m_selected = false;
    m_animProgress = 0.0;
    update();
}

void KTileWidget::enterEvent(QEnterEvent *)
{
    Q_EMIT hovered(m_row, m_col);
}

void KTileWidget::mouseReleaseEvent(QMouseEvent *e)
{
    const QPoint mousePos = e->position().toPoint();
    if (mousePos.x() < 0 || mousePos.x() > width() || mousePos.y() < 0 || mousePos.y() > height()) {
        return;
    }

    if (e->button() == Qt::LeftButton && clicksAllowed) {
        e->accept();
        Q_EMIT clicked(m_row, m_col);
    }
}

static SVGElement ownerToElement(Owner o)
{
    switch (o) {
    case Owner::One:
        return SVGElement::Player1;
    case Owner::Two:
        return SVGElement::Player2;
    case Owner::Wall:
        return SVGElement::Wall;
    default:
        return SVGElement::Neutral;
    }
}

void KTileWidget::paintEvent(QPaintEvent * /* ev unused */)
{
    if ((pixmaps == nullptr) || (pixmaps->isEmpty())) {
        return;
    }

    int w = this->width();
    int h = this->height();

    QPainter p(this);

    SVGElement el = ownerToElement(m_owner);
    int pmw = pixmaps->at(int(el)).width();
    int pmh = pixmaps->at(int(el)).height();
    int ox = (w - pmw) / 2;
    int oy = (h - pmh) / 2;

    // Draw selection marker or effect overlay
    if (m_selected) {
        p.drawPixmap(ox, oy, pixmaps->at(int(el)));
        p.drawPixmap(ox, oy, pixmaps->at(int(SVGElement::BlinkDark)));
    } else {
        switch (m_effect) {
        case Effect::None:
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));
            break;
        case Effect::Light:
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));
            p.drawPixmap(ox, oy, pixmaps->at(int(SVGElement::BlinkLight)));
            break;
        case Effect::Dark:
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));
            p.drawPixmap(ox, oy, pixmaps->at(int(SVGElement::BlinkDark)));
            break;
        case Effect::CloneHighlight:
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));
            p.drawPixmap(ox, oy, pixmaps->at(int(m_playerHighlight == 1 ? SVGElement::P1CloneTarget : SVGElement::P2CloneTarget)));
            break;
        case Effect::JumpHighlight:
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));
            p.drawPixmap(ox, oy, pixmaps->at(int(m_playerHighlight == 1 ? SVGElement::P1JumpTarget : SVGElement::P2JumpTarget)));
            break;
        case Effect::ZoomIn:
        case Effect::ZoomOut: {
            // Draw neutral background behind the zooming tile
            p.drawPixmap(ox, oy, pixmaps->at(int(SVGElement::Neutral)));

            // Scale the owner pixmap based on zoom progress
            double scale = (m_effect == Effect::ZoomIn) ? m_animProgress : (1.0 - m_animProgress);
            if (scale > 0.0) {
                p.save();
                p.translate(w / 2.0, h / 2.0);
                p.scale(scale, scale);
                p.translate(-w / 2.0, -h / 2.0);
                p.drawPixmap(ox, oy, pixmaps->at(int(el)));
                p.restore();
            }
        } break;
        case Effect::Converting: {
            SVGElement oldEl = (el == SVGElement::Player1) ? SVGElement::Player2 : SVGElement::Player1;

            // Build diagonal clip polygon: the line x + y = d sweeps top-left to bottom-right
            double d = m_animProgress * (w + h);
            QPolygonF clipPoly;
            if (d <= qMin(w, h)) {
                clipPoly << QPointF(0, 0) << QPointF(d, 0) << QPointF(0, d);
            } else if (d <= qMax(w, h)) {
                if (w <= h) {
                    clipPoly << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, d - w) << QPointF(0, d);
                } else {
                    clipPoly << QPointF(0, 0) << QPointF(d, 0) << QPointF(d - h, h) << QPointF(0, h);
                }
            } else if (d < w + h) {
                clipPoly << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, d - w) << QPointF(d - h, h) << QPointF(0, h);
            } else {
                clipPoly << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, h) << QPointF(0, h);
            }

            QPainterPath fullRect;
            fullRect.addRect(0, 0, w, h);
            QPainterPath revealPath;
            revealPath.addPolygon(clipPoly);
            QPainterPath concealPath = fullRect - revealPath;

            // Draw old owner clipped to the unrevealed region
            p.setClipPath(concealPath);
            p.drawPixmap(ox, oy, pixmaps->at(int(oldEl)));

            // Draw new owner clipped to the revealed region
            p.setClipPath(revealPath);
            p.drawPixmap(ox, oy, pixmaps->at(int(el)));

            p.setClipping(false);
        } break;
        }
    }

    p.end();
}

#include "moc_ktilewidget.cpp"
