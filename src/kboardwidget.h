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

    ~KBoardWidget() override = default;

    void deselectAll();
    void reset();
    const QPixmap &playerPixmap(int p);
    void setWaitCursor();
    void setNormalCursor();
    bool loadSettings();
    virtual void setSize(int dim);
    int killAnimation();
    void clearValidMoveHighlights();
    void showPopup(const QString &message);
    void hidePopup();

    // Conversion functions because the external world uses x,y but we use a contiguous array.
    void timedTileHighlight(QPoint p)
    {
        timedTileHighlight(pointToIndex(p));
    }
    void displayTile(QPoint p, Owner owner)
    {
        displayTile(pointToIndex(p), owner);
    }
    void selectTile(QPoint p) {
        selectTile(pointToIndex(p));
    }
    void startComputerNextMoveAnimation(QPoint origin, QPoint dest = QPoint(-1, -1))
    {
        startComputerNextMoveAnimation(pointToIndex(origin), pointToIndex(dest));
    }
    void startCloneAnimation(QPoint dest, const QList<QPoint> &convertedTiles, Owner oldOwner)
    {
        startMoveAnimation(pointToIndex(dest), -1, pointsToIndices(convertedTiles), oldOwner);
    }
    void startJumpAnimation(QPoint dest, QPoint origin, const QList<QPoint> &convertedTiles, Owner oldOwner)
    {
        startMoveAnimation(pointToIndex(dest), pointToIndex(origin), pointsToIndices(convertedTiles), oldOwner);
    }
    void highlightValidMoves(int player, const QList<QPoint> &cloneTargets, const QList<QPoint> &jumpTargets) {
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

protected:
    QSize sizeHint() const override;
    virtual void initTiles();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    int pointToIndex(QPoint p)
    {
        if (p.x() < 0 || p.y() < 0) {
            return -1;
        }
        return p.x() * m_size + p.y();
    }
    QList<int> pointsToIndices(const QList<QPoint>& convertedTiles) {
        QList<int> convertedIndices;
        for (const QPoint& p : convertedTiles) {
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
    void startMoveAnimation(int zoomInTile, int zoomOutTile, const QList<int> &convertedIndices, Owner oldOwner);
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
