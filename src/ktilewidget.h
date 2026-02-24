/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QFrame>

class QMouseEvent;
class QPaintEvent;

enum class Owner {
    Nobody,
    One,
    Two,
    Wall,
};

enum class SVGElement {
    Neutral,
    Player1,
    Player2,
    Wall,
    BlinkLight,
    BlinkDark,
    P1CloneTarget,
    P1JumpTarget,
    P2CloneTarget,
    P2JumpTarget,
    FirstElement = Neutral,
    LastElement = P2JumpTarget
};

class KTileWidget : public QFrame
{
    Q_OBJECT

public:
    explicit KTileWidget(QWidget *parent, int row, int col);
    ~KTileWidget() override = default;

    static void enableClicks(bool flag);
    static void setPixmaps(QList<QPixmap> *ptr);

    void setOwner(Owner newOwner)
    {
        if (newOwner != m_owner) {
            m_owner = newOwner;
            update();
        }
    }

    void setNeutral()
    {
        if (m_effect != Effect::None) {
            m_effect = Effect::None;
            update();
        }
    }

    void setLight()
    {
        if (m_effect != Effect::Light) {
            m_effect = Effect::Light;
            update();
        }
    }

    void setDark()
    {
        if (m_effect != Effect::Dark) {
            m_effect = Effect::Dark;
            update();
        }
    }

    void setCloneTarget(int player)
    {
        if (m_effect != Effect::CloneHighlight || m_playerHighlight != player) {
            m_playerHighlight = player;
            m_effect = Effect::CloneHighlight;
            update();
        }
    }

    void setJumpTarget(int player)
    {
        if (m_effect != Effect::JumpHighlight || m_playerHighlight != player) {
            m_playerHighlight = player;
            m_effect = Effect::JumpHighlight;
            update();
        }
    }

    void setSelected(bool selected)
    {
        if (selected != m_selected) {
            m_selected = selected;
            update();
        }
    }

    void startConversion(Owner fromOwner);
    void setConversionProgress(qreal t);
    void endConversion();

    void startZoom(bool zoomIn);
    void setZoomProgress(qreal t);
    void endZoom();

public Q_SLOTS:
    virtual void reset();

Q_SIGNALS:
    void clicked(int row, int column);

protected:
    void mouseReleaseEvent(QMouseEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    enum class Effect {
        None,
        Light,
        Dark,
        CloneHighlight,
        JumpHighlight
    };

    int m_row;
    int m_col;

    Owner m_owner;
    Effect m_effect;
    int m_playerHighlight;
    bool m_selected;

    Owner m_conversionFrom;
    qreal m_conversionProgress;
    bool m_converting;

    bool m_zooming;
    bool m_zoomIn;
    qreal m_zoomProgress;

    static QList<QPixmap> *pixmaps;
    static bool clicksAllowed;
};
