/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ktilewidget.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
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

KTileWidget::~KTileWidget()
{
}

void KTileWidget::reset()
{
    m_owner = Owner::Nobody;
    m_playerHighlight = 0;
    m_effect = Effect::None;
    m_selected = false;
    update();
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

void KTileWidget::paintEvent(QPaintEvent * /* ev unused */)
{
    if ((pixmaps == nullptr) || (pixmaps->isEmpty())) {
        return;
    }

    int w = this->width();
    int h = this->height();

    QPainter p(this);

    // Draw tile background based on owner
    SVGElement el = SVGElement::Neutral;
    if (m_owner == Owner::One)
        el = SVGElement::Player1;
    else if (m_owner == Owner::Two)
        el = SVGElement::Player2;
    else if (m_owner == Owner::Wall)
        el = SVGElement::Wall;

    int pmw = pixmaps->at(int(el)).width();
    int pmh = pixmaps->at(int(el)).height();
    p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(el)));

    // Draw selection marker if this tile is selected
    if (m_selected) {
        p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(SVGElement::BlinkDark)));
    } else {
        switch (m_effect) {
        case Effect::Light:
            p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(SVGElement::BlinkLight)));
            break;
        case Effect::Dark:
            p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(SVGElement::BlinkDark)));
            break;
        case Effect::CloneHighlight:
            p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(m_playerHighlight == 1 ? SVGElement::P1CloneTarget : SVGElement::P2CloneTarget)));
            break;
        case Effect::JumpHighlight:
            p.drawPixmap((w - pmw) / 2, (h - pmh) / 2, pixmaps->at(int(m_playerHighlight == 1 ? SVGElement::P1JumpTarget : SVGElement::P2JumpTarget)));
            break;
        default:
            break;
        }
    }

    p.end();
}

#include "moc_ktilewidget.cpp"
