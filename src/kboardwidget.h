/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ktilewidget.h"

#include <QList>
#include <QSvgRenderer>
#include <QWidget>

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
    explicit KBoardWidget(QWidget *parent = nullptr);

    ~KBoardWidget() override = default;

    void reset();
    const QPixmap &playerPixmap(int p);
    void setWaitCursor();
    void setNormalCursor();
    bool loadSettings();
    void setSize(int dim);
    void killAnimation();
    void clearValidMoveHighlights();
    void deselectAll();
    void showPopup(const QString &message);
    void hidePopup();

    // Conversion functions because the external world uses x,y but we use a linear array
    void timedTileHighlight(QPoint p)
    {
        timedTileHighlight(pointToIndex(p));
    }
    void displayTile(QPoint p, Owner owner)
    {
        displayTile(pointToIndex(p), owner);
    }
    void selectTile(QPoint p)
    {
        selectTile(pointToIndex(p));
    }
    void startComputerNextMoveAnimation(QPoint origin, QPoint dest = QPoint(-1, -1))
    {
        startComputerNextMoveAnimation(pointToIndex(origin), pointToIndex(dest));
    }
    void startCloneAnimation(QPoint dest, const QList<QPoint> &convertedTiles, const QList<QPoint> &autofilledTiles, Owner owner)
    {
        startMoveAnimation(pointToIndex(dest), -1, pointsToIndices(convertedTiles), pointsToIndices(autofilledTiles), owner);
    }
    void startJumpAnimation(QPoint dest, QPoint origin, const QList<QPoint> &convertedTiles, const QList<QPoint> &autofilledTiles, Owner owner)
    {
        startMoveAnimation(pointToIndex(dest), pointToIndex(origin), pointsToIndices(convertedTiles), pointsToIndices(autofilledTiles), owner);
    }
    void highlightValidMoves(int player, const QList<QPoint> &cloneTargets, const QList<QPoint> &jumpTargets)
    {
        highlightValidMoves(player, pointsToIndices(cloneTargets), pointsToIndices(jumpTargets));
    }

private Q_SLOTS:
    void nextAnimationStep();
    void highlightDone();
    void nextMoveAnimationStep();
    bool checkClick(int x, int y);

Q_SIGNALS:
    void animationDone(int index);
    void mouseClick(int x, int y);
    void tileHovered(int x, int y);

protected:
    QSize sizeHint() const override;
    void initTiles();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int pointToIndex(QPoint p)
    {
        if (p.x() < 0 || p.y() < 0) {
            return -1;
        }
        return p.x() * m_size + p.y();
    }
    QList<int> pointsToIndices(const QList<QPoint> &convertedTiles)
    {
        QList<int> convertedIndices;
        for (const QPoint &p : convertedTiles) {
            convertedIndices << pointToIndex(p);
        }
        return convertedIndices;
    }
    void init();
    void setPopup();
    void makeSVGBackground(const int w, const int h);
    void makeSVGTiles(const int width);
    void colorImage(QImage &img, const QColor &c, const int w);
    void reCalculateGraphics(const int w, const int h);

    void displayTile(int index, Owner owner);
    void selectTile(int index);
    void startComputerNextMoveAnimation(int originTile, int destTile = -1);
    void startMoveAnimation(int zoomInTile, int zoomOutTile, const QList<int> &convertedIndices, const QList<int> &autofilledIndices, Owner owner);
    void timedTileHighlight(int index);
    void highlightValidMoves(int player, const QList<int> &cloneTargets, const QList<int> &jumpTargets);

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

    QTimer *m_blinkTimer;
    int m_blinkCount;
    int m_blinkOrigin;
    int m_blinkDest;

    QTimer *m_highlightTimer;
    int m_highlighted;

    QTimer *m_animationTimer;
    QList<int> m_convertedTiles;
    QList<int> m_autofilledTiles;
    int m_animationStep;
    Owner m_animationNewOwner;

    int m_zoomInTile;
    int m_zoomOutTile;

    QList<int> m_validMoveHighlights; // Indices of highlighted tiles

    QLabel *m_popup;
};
