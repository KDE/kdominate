/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 * SPDX-FileCopyrightText: 2012-2013 Ian Wadham <iandw.au@gmail.com>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kboardwidget.h"

#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QTimer>

#include "prefs.h"

#include "kdominate_debug.h"

const int kHighlightTimeMillis = 1500;
const int kBlinkCountComputerMove = 4; // also used for hints
const int kBlinkDurationMillis = 250;
const int kAnimationStepMillis = 20;
const int kZoomSteps = 10; // 20ms * 10 steps = 200ms
const int kConversionSteps = 16; // 20ms * 16 steps = 320ms

KBoardWidget::KBoardWidget(const int d, QWidget *parent)
    : QWidget(parent)
    , m_size(d)
    , m_popup(new QLabel(this))
{
    qCDebug(KDOMINATE_LOG) << "CONSTRUCT KBoardWidget: size" << m_size;

    svg.load(QStringLiteral(":/graphics.svg"));

    animationCount = 0;
    m_blinkOrigin = -1;
    m_blinkDest = -1;

    setMinimumSize(200, 200);
    color1 = Prefs::color1();
    color2 = Prefs::color2();
    color0 = Prefs::color0();
    m_theme = Theme(Prefs::themeIndex());
    // We don't know the actual size until setSize is called, but we need to generate
    // the tiles already since we use them in the status bar. Hardcode some small size.
    makeSVGTiles(30);

    initTiles();

    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(kBlinkDurationMillis);

    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setInterval(kHighlightTimeMillis);
    m_highlighted = -1;

    m_conversionTimer = new QTimer(this);
    m_conversionTimer->setInterval(kAnimationStepMillis);
    m_conversionStep = 0;
    m_conversionNewOwner = Owner::Nobody;
    m_zoomInTile = -1;
    m_zoomOutTile = -1;

    connect(m_animationTimer, &QTimer::timeout, this, &KBoardWidget::nextAnimationStep);
    connect(m_highlightTimer, &QTimer::timeout, this, &KBoardWidget::highlightDone);
    connect(m_conversionTimer, &QTimer::timeout, this, &KBoardWidget::nextMoveAnimationStep);
    setNormalCursor();
    setPopup();
}

bool KBoardWidget::loadSettings()
{
    qCDebug(KDOMINATE_LOG) << "LOAD VIEW SETTINGS";
    bool reColorTiles = ((color1 != Prefs::color1()) || (color2 != Prefs::color2()) || (color0 != Prefs::color0()) || (int(m_theme) != Prefs::themeIndex()));

    color1 = Prefs::color1();
    color2 = Prefs::color2();
    color0 = Prefs::color0();
    m_theme = Theme(Prefs::themeIndex());

    if (reColorTiles) {
        makeSVGTiles(tileSize);
        for (KTileWidget *tile : std::as_const(tiles)) {
            tile->update();
        }
    }
    return reColorTiles;
}

void KBoardWidget::reset()
{
    for (KTileWidget *tile : std::as_const(tiles)) {
        tile->reset();
    }

    KTileWidget::enableClicks(true);
}

void KBoardWidget::displayTile(int index, Owner owner)
{
    tiles.at(index)->setOwner(owner);
}

void KBoardWidget::highlightTile(int index, bool highlight)
{
    if (highlight) {
        tiles.at(index)->setDark();
    } else {
        tiles.at(index)->setNeutral();
    }
}

void KBoardWidget::timedTileHighlight(int index)
{
    if (m_highlighted > 0) {
        highlightDone();
    }
    tiles.at(index)->setDark();
    m_highlighted = index;
    m_highlightTimer->start();
}

void KBoardWidget::selectTile(int index)
{
    deselectAll();
    tiles.at(index)->setSelected(true);
}

void KBoardWidget::deselectAll()
{
    for (KTileWidget *tile : std::as_const(tiles)) {
        tile->setSelected(false);
    }
}

void KBoardWidget::highlightDone()
{
    tiles.at(m_highlighted)->setNeutral();
    m_highlightTimer->stop();
    m_highlighted = -1;
}

void KBoardWidget::setSize(int d)
{
    if (d != m_size) {
        m_size = d;
        initTiles();
        reCalculateGraphics(width(), height());
        reset();
    }
}

void KBoardWidget::setWaitCursor()
{
    setCursor(Qt::BusyCursor);
}

void KBoardWidget::setNormalCursor()
{
    setCursor(Qt::PointingHandCursor);
}

bool KBoardWidget::checkClick(int x, int y)
{
    qCDebug(KDOMINATE_LOG) << "Emit mouseClick (" << x << y << ")";
    Q_EMIT mouseClick(x, y);
    return false;
}

void KBoardWidget::initTiles()
{
    qDeleteAll(tiles);
    tiles.clear();

    KTileWidget::setPixmaps(&graphicElements);

    int nTiles = m_size * m_size;
    for (int n = 0; n < nTiles; n++) {
        KTileWidget *tile = new KTileWidget(this, n / m_size, n % m_size);
        tiles.append(tile);
        connect(tile, &KTileWidget::clicked, this, &KBoardWidget::checkClick);
        tile->show();
    }
}

const QPixmap &KBoardWidget::playerPixmap(int p)
{
    return ((p == 1) ? graphicElements[int(SVGElement::Player1)] : graphicElements[int(SVGElement::Player2)]);
}

void KBoardWidget::makeSVGBackground(const int w, const int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    img.fill(0);
    svg.render(&p, QStringLiteral("background"));
    p.end();
    background = QPixmap::fromImage(img);
}

void KBoardWidget::makeSVGTiles(const int width)
{
    QImage img(width, width, QImage::Format_ARGB32_Premultiplied);
    QPainter q;

    QRectF rect(0, 0, width, width);
    qreal pc = 20.0; // % radius on corners
    graphicElements.clear();

    QString svgThemeTile = m_theme == Theme::Glass ? QStringLiteral("glass") : QStringLiteral("plastic");

    for (int i = int(SVGElement::FirstElement); i <= int(SVGElement::LastElement); i++) {
        SVGElement element = SVGElement(i);
        q.begin(&img);
        q.setPen(Qt::NoPen);
        img.fill(0);

        switch (element) {
        case SVGElement::Neutral:
            if (m_theme == Theme::Simple) {
                q.setPen(QPen(QColor(0, 0, 0, 40), width * 0.04));
                q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            } else {
                svg.render(&q, svgThemeTile);
            }
            break;
        case SVGElement::Player1:
            q.setBrush(color1);
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            if (m_theme == Theme::Simple) {
                q.setPen(QPen(color1.darker(130), width * 0.04));
                q.setBrush(Qt::NoBrush);
                q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            } else {
                svg.render(&q, svgThemeTile);
            }
            break;
        case SVGElement::Player2:
            q.setBrush(color2);
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            if (m_theme == Theme::Simple) {
                q.setPen(QPen(color2.darker(130), width * 0.04));
                q.setBrush(Qt::NoBrush);
                q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            } else {
                svg.render(&q, svgThemeTile);
            }
            break;
        case SVGElement::Wall:
            q.setBrush(color0);
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            if (m_theme == Theme::Simple) {
                q.setPen(QPen(color0.darker(130), width * 0.04));
                q.setBrush(Qt::NoBrush);
                q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            } else {
                svg.render(&q, svgThemeTile);
            }
            break;
        case SVGElement::BlinkLight:
            svg.render(&q, QStringLiteral("blink_light"));
            break;
        case SVGElement::BlinkDark:
            svg.render(&q, QStringLiteral("blink_dark"));
            break;
        case SVGElement::P1CloneTarget:
            q.setBrush(QColor(color1.red(), color1.green(), color1.blue(), 100));
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            break;
        case SVGElement::P1JumpTarget:
            q.setBrush(QColor(color1.red(), color1.green(), color1.blue(), 50));
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            break;
        case SVGElement::P2CloneTarget:
            q.setBrush(QColor(color2.red(), color2.green(), color2.blue(), 100));
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            break;
        case SVGElement::P2JumpTarget:
            q.setBrush(QColor(color2.red(), color2.green(), color2.blue(), 50));
            q.drawRoundedRect(rect, pc, pc, Qt::RelativeSize);
            break;
        default:
            break;
        }
        q.end();
        graphicElements.append(QPixmap::fromImage(img));
    }
}

void KBoardWidget::colorImage(QImage &img, const QColor &c, const int w)
{
    QRgb rgba = c.rgba();
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < w; j++) {
            if (img.pixel(i, j) != 0) {
                img.setPixel(i, j, rgba);
            }
        }
    }
}

void KBoardWidget::paintEvent(QPaintEvent * /* event unused */)
{
    QPainter p(this);
    p.drawPixmap(0, 0, background);
}

void KBoardWidget::resizeEvent(QResizeEvent *event)
{
    reCalculateGraphics(event->size().width(), event->size().height());
}

void KBoardWidget::reCalculateGraphics(const int w, const int h)
{
    int boxSize = qMin(w, h);
    int frameWidth = boxSize / 30;
    boxSize = boxSize - (2 * frameWidth);
    tileSize = (boxSize / m_size);
    boxSize = (tileSize * m_size);
    topLeft.setX((w - boxSize) / 2);
    topLeft.setY((h - boxSize) / 2);

    makeSVGBackground(w, h);
    makeSVGTiles(tileSize);
    for (int x = 0; x < m_size; x++) {
        for (int y = 0; y < m_size; y++) {
            int index = x * m_size + y;
            tiles.at(index)->move(topLeft.x() + (x * tileSize), topLeft.y() + (y * tileSize));
            tiles.at(index)->resize(tileSize, tileSize);
        }
    }
    setPopup();
}

QSize KBoardWidget::sizeHint() const
{
    return QSize(400, 400);
}

void KBoardWidget::startComputerNextMoveAnimation(int originTile, int destTile)
{
    m_blinkOrigin = originTile;
    m_blinkDest = destTile;
    animationCount = kBlinkCountComputerMove;
    tiles.at(m_blinkOrigin)->setLight();
    if (m_blinkDest >= 0 && m_blinkDest < tiles.size()) {
        tiles.at(m_blinkDest)->setLight();
    }

    m_animationTimer->start();
}

void KBoardWidget::nextAnimationStep()
{
    animationCount--;
    if (animationCount < 1) {
        m_animationTimer->stop();
        tiles.at(m_blinkOrigin)->setNeutral();
        if (m_blinkDest >= 0 && m_blinkDest < tiles.size()) {
            tiles.at(m_blinkDest)->setNeutral();
        }
        Q_EMIT animationDone(m_blinkOrigin);
        return;
    }
    if (animationCount % 2 == 1) {
        tiles.at(m_blinkOrigin)->setDark();
        if (m_blinkDest >= 0 && m_blinkDest < tiles.size()) {
            tiles.at(m_blinkDest)->setDark();
        }
    } else {
        tiles.at(m_blinkOrigin)->setLight();
        if (m_blinkDest >= 0 && m_blinkDest < tiles.size()) {
            tiles.at(m_blinkDest)->setLight();
        }
    }
}

void KBoardWidget::startMoveAnimation(int zoomInTile, int zoomOutTile, const QList<int> &convertedIndices, Owner newOwner)
{
    m_zoomInTile = zoomInTile;
    m_zoomOutTile = zoomOutTile;
    m_convertedTiles = convertedIndices;
    m_conversionNewOwner = newOwner;
    m_conversionStep = 0;

    if (m_zoomInTile >= 0) {
        tiles.at(m_zoomInTile)->startZoom(true);
    }
    if (m_zoomOutTile >= 0) {
        tiles.at(m_zoomOutTile)->startZoom(false);
    }
    // Conversions begin after the zoom phase completes
    m_conversionTimer->start();
}

void KBoardWidget::nextMoveAnimationStep()
{
    m_conversionStep++;

    // Phase 1: zoom progress (steps 1..kZoomSteps)
    if (m_conversionStep <= kZoomSteps) {
        qreal progress = qreal(m_conversionStep) / qreal(kZoomSteps);
        if (m_zoomInTile >= 0) {
            tiles.at(m_zoomInTile)->setZoomProgress(progress);
        }
        if (m_zoomOutTile >= 0) {
            tiles.at(m_zoomOutTile)->setZoomProgress(progress);
        }
        return;
    }

    // Zoom cleanup (step kZoomSteps+1): end zoom, then start conversion or finish
    if (m_conversionStep == kZoomSteps + 1) {
        if (m_zoomInTile >= 0) {
            tiles.at(m_zoomInTile)->endZoom();
            m_zoomInTile = -1;
        }
        if (m_zoomOutTile >= 0) {
            tiles.at(m_zoomOutTile)->endZoom();
            tiles.at(m_zoomOutTile)->setOwner(Owner::Nobody);
            m_zoomOutTile = -1;
        }
        if (m_convertedTiles.isEmpty()) {
            m_conversionTimer->stop();
            Q_EMIT animationDone(-1);
            return;
        }
        // Start conversion phase: set new owner now, then animate the swipe
        Owner oldOwner = (m_conversionNewOwner == Owner::One) ? Owner::Two : Owner::One;
        for (int idx : std::as_const(m_convertedTiles)) {
            tiles.at(idx)->setOwner(m_conversionNewOwner);
            tiles.at(idx)->startConversion(oldOwner);
        }
        return;
    }

    // Phase 2: conversion progress (steps kZoomSteps+2..kZoomSteps+1+kConversionSteps)
    int convStep = m_conversionStep - kZoomSteps - 1;
    if (convStep <= kConversionSteps) {
        qreal progress = qreal(convStep) / qreal(kConversionSteps);
        for (int idx : std::as_const(m_convertedTiles)) {
            tiles.at(idx)->setConversionProgress(progress);
        }
        return;
    }

    // Conversion cleanup (one tick after final progress frame)
    m_conversionTimer->stop();
    for (int idx : std::as_const(m_convertedTiles)) {
        tiles.at(idx)->endConversion();
    }
    m_convertedTiles.clear();
    Q_EMIT animationDone(-1);
}

int KBoardWidget::killAnimation()
{
    if (m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }
    if (m_conversionTimer->isActive()) {
        m_conversionTimer->stop();
        if (m_zoomInTile >= 0) {
            tiles.at(m_zoomInTile)->endZoom();
            m_zoomInTile = -1;
        }
        if (m_zoomOutTile >= 0) {
            tiles.at(m_zoomOutTile)->endZoom();
            m_zoomOutTile = -1;
        }
        for (int idx : std::as_const(m_convertedTiles)) {
            tiles.at(idx)->endConversion();
        }
        m_convertedTiles.clear();
    }
    if (m_blinkOrigin >= 0 && m_blinkOrigin < tiles.size()) {
        tiles.at(m_blinkOrigin)->setNeutral();
    }
    if (m_blinkDest >= 0 && m_blinkDest < tiles.size()) {
        tiles.at(m_blinkDest)->setNeutral();
    }
    m_blinkDest = -1;
    return m_blinkOrigin;
}

void KBoardWidget::highlightValidMoves(int player, const QList<int> &cloneTargets, const QList<int> &jumpTargets)
{
    clearValidMoveHighlights();
    for (int idx : cloneTargets) {
        if (idx >= 0 && idx < tiles.size()) {
            tiles.at(idx)->setCloneTarget(player);
            m_validMoveHighlights.append(idx);
        }
    }
    for (int idx : jumpTargets) {
        if (idx >= 0 && idx < tiles.size()) {
            tiles.at(idx)->setJumpTarget(player);
            m_validMoveHighlights.append(idx);
        }
    }
}

void KBoardWidget::clearValidMoveHighlights()
{
    for (int idx : std::as_const(m_validMoveHighlights)) {
        if (idx >= 0 && idx < tiles.size()) {
            tiles.at(idx)->setNeutral();
        }
    }
    m_validMoveHighlights.clear();
}

void KBoardWidget::setPopup()
{
    QFont f;
    f.setPixelSize((int)(height() * 0.04 + 0.5));
    f.setWeight(QFont::Bold);
    f.setStretch(QFont::Expanded);
    m_popup->setStyleSheet(QStringLiteral("QLabel { color : rgba(255, 255, 255, 75%); }"));
    m_popup->setFont(f);
    m_popup->resize(width(), (int)(height() * 0.08 + 0.5));
    m_popup->setAlignment(Qt::AlignCenter);
}

void KBoardWidget::showPopup(const QString &message)
{
    m_popup->setText(message);
    m_popup->move((this->width() - m_popup->width()) / 2, (this->height() - m_popup->height()) / 2 + (tiles.at(0)->height() / 5));
    m_popup->raise();
    m_popup->show();
    update();
}

void KBoardWidget::hidePopup()
{
    m_popup->hide();
    update();
}

#include "moc_kboardwidget.cpp"
