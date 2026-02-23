/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QSvgRenderer>
#include <QWidget>

#include "ktilewidget.h"

class QPaintEvent;
class QResizeEvent;
class QTimer;
class QLabel;

enum class Theme {
    Simple,
    Plastic,
    Glass,
};

class KBoardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit KBoardWidget(const int dim = 1, QWidget *parent = nullptr);

    ~KBoardWidget() override;

    void timedTileHighlight(int index);
    void deselectAll();
    void reset();
    const QPixmap &playerPixmap(int p);
    void setWaitCursor();
    void setNormalCursor();
    bool loadSettings();
    virtual void setSize(int dim);
    void startComputerMoveAnimation(int originTile, int destTile = -1);
    void startMoveAnimation(int zoomInTile, int zoomOutTile, const QList<int> &convertedIndices, Owner oldOwner);
    int killAnimation();
    void highlightValidMoves(int player, const QList<int> &cloneTargets, const QList<int> &jumpTargets);
    void clearValidMoveHighlights();
    void showPopup(const QString &message);
    void hidePopup();

    // Conversion functions because the external world uses x,y but we use a contiguous array.
    void displayTile(QPoint p, Owner owner)
    {
        displayTile(p.x() * m_size + p.y(), owner);
    }

    void highlightTile(QPoint p, bool highlight)
    {
        highlightTile(p.x() * m_size + p.y(), highlight);
    }

    void selectTile(QPoint p)
    {
        selectTile(p.x() * m_size + p.y());
    }

private Q_SLOTS:
    void nextAnimationStep();
    void highlightDone();
    void nextMoveAnimationStep();
    bool checkClick(int x, int y);

Q_SIGNALS:
    void animationDone(int index);
    void mouseClick(int x, int y);

protected:
    QSize sizeHint() const override;
    virtual void initTiles();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void init();
    void setPopup();
    void makeSVGBackground(const int w, const int h);
    void makeSVGTiles(const int width);
    void colorImage(QImage &img, const QColor &c, const int w);
    void reCalculateGraphics(const int w, const int h);

    void displayTile(int index, Owner owner);
    void selectTile(int index);
    void highlightTile(int index, bool highlight);

    QSvgRenderer svg;
    QPixmap background;
    QList<QPixmap> graphicElements;
    QColor color1;
    QColor color2;
    QColor color0;
    Theme m_theme;

    QPoint topLeft;
    int tileSize;

    int m_size;
    QList<KTileWidget *> tiles;

    QTimer *m_animationTimer;
    int m_blinkOrigin;
    int m_blinkDest;
    int animationCount;
    int animationTime;

    QTimer *m_highlightTimer;
    int m_highlighted;

    QTimer *m_conversionTimer;
    QList<int> m_convertedTiles;
    int m_conversionStep;
    Owner m_conversionNewOwner;

    int m_zoomInTile;
    int m_zoomOutTile;

    QList<int> m_validMoveHighlights; // Indices of highlighted tiles

    QLabel *m_popup;
};
