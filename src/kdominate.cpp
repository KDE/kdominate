/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kdominate.h"

#include "kboardwidget.h"
#include "kdominate_board.h"
#include "kdominate_debug.h"
#include "prefs.h"
#include "settingswidget.h"

#include <KActionCollection>
#include <KGameStandardAction>
#include <KLocalizedString>
#include <KStandardAction>

#include <QAction>
#include <QFile>
#include <QStatusBar>
#include <QTextStream>
#include <QWidgetAction>

const int kTimedStatusBarMessageDurationMillis = 2000;

KDominate::KDominate()
{
    m_view = new KBoardWidget(this);
    m_game = new Game(m_view, this);

    connect(m_game, &Game::statusUpdated, this, &KDominate::updateStatus);
    connect(m_game, &Game::buttonChange, this, &KDominate::changeButton);
    connect(m_game, &Game::statusMessage, this, &KDominate::statusMessage);

    setCentralWidget(m_view);

    // init statusbar
    m_p1Icon = new QLabel();
    m_p1Count = new QLabel();
    m_p2Icon = new QLabel();
    m_p2Count = new QLabel();
    m_currentPlayerIcon = new QLabel();
    m_currentPlayerLabel = new QLabel();
    statusBar()->addPermanentWidget(m_p1Icon);
    statusBar()->addPermanentWidget(m_p1Count);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral("|")));
    statusBar()->addPermanentWidget(m_p2Icon);
    statusBar()->addPermanentWidget(m_p2Count);
    statusBar()->addPermanentWidget(new QLabel(QStringLiteral(" - ")));
    statusBar()->addPermanentWidget(m_currentPlayerLabel);
    statusBar()->addPermanentWidget(m_currentPlayerIcon);
    statusBar()->addPermanentWidget(new QWidget()); // Padding

    connect(m_view, &KBoardWidget::tileHovered, this, &KDominate::showTileCoords);
    updateStatus();

    {
        actionButton = new QPushButton(this);
        actionButton->setObjectName(QStringLiteral("ActionButton"));
        connect(actionButton, &QAbstractButton::clicked, this, [this]() {
            m_game->gameActions(Action::BUTTON);
        });

        QWidgetAction *widgetAction = new QWidgetAction(this);
        widgetAction->setDefaultWidget(actionButton);
        actionCollection()->addAction(QStringLiteral("action_button"), widgetAction);
        changeButton(true, true); // Load the button's style sheet.
        changeButton(false); // Set the button to be inactive.
    }

    {
        QAction *action = KStandardAction::preferences(
            this,
            [this]() {
                m_game->showSettingsDialog();
            },
            actionCollection());
        action->setIconText(i18n("Settings"));
    }

    {
        QAction *action = KGameStandardAction::gameNew(
            this,
            [this]() {
                m_game->gameActions(Action::NEW);
            },
            this);
        actionCollection()->addAction(action->objectName(), action);
    }

    {
        QAction *action = KGameStandardAction::hint(
            this,
            [this]() {
                m_game->gameActions(Action::HINT);
            },
            this);
        actionCollection()->addAction(action->objectName(), action);

        connect(m_game, &Game::setHintAvailable, action, &QAction::setEnabled);
    }

    {
        QAction* undoAction = KGameStandardAction::undo(
            this,
            [this]() {
                m_game->gameActions(Action::UNDO);
            },
            this);
        actionCollection()->addAction(undoAction->objectName(), undoAction);

        QAction* redoAction = KGameStandardAction::redo(
            this,
            [this]() {
                m_game->gameActions(Action::REDO);
            },
            this);
        actionCollection()->addAction(redoAction->objectName(), redoAction);

        connect(m_game, &Game::setUndoRedoAvailable, this, [undoAction, redoAction](bool canUndo, bool canRedo) {
            undoAction->setEnabled(canUndo);
            redoAction->setEnabled(canRedo);
        });
    }

    {
        QAction *action = KGameStandardAction::quit(this, &KDominate::close, this);
        actionCollection()->addAction(action->objectName(), action);
    }

    setupGUI();


    m_game->gameActions(Action::NEW);
}

KDominate::~KDominate()
{
    delete m_game;
}

void KDominate::changeButton(bool enabled, bool stop, const QString &caption)
{
    // Action button's style sheet: parameters for blue and red colors
    static QString buttonLook = QStringLiteral(
        "QPushButton#ActionButton { color: white; background-color: %1; "
        "border-style: outset; border-width: 2px; border-radius: 10px; "
        "border-color: beige; font: bold 14px; min-width: 10em; "
        "padding: 6px; margin: 5px; margin-left: 10px; } "
        "QPushButton#ActionButton:pressed { background-color: %2; "
        "border-style: inset; } "
        "QPushButton#ActionButton:disabled { color: white;"
        "border-color: beige; background-color: steelblue; }");
    if (enabled && stop) { // Red look (stop something).
        actionButton->setStyleSheet(buttonLook.arg(QStringLiteral("rgb(210, 0, 0)")).arg(QStringLiteral("rgb(180, 0, 0)")));
    } else if (enabled) { // Blue look (continue something).
        actionButton->setStyleSheet(buttonLook.arg(QStringLiteral("rgb(0, 170, 0)")).arg(QStringLiteral("rgb(0, 150, 0)")));
    }
    actionButton->setText(caption);
    actionButton->setEnabled(enabled);
}

void KDominate::updateStatus()
{
    const int iconSize = 16;
    QPixmap p1Scaled = m_view->playerPixmap(1).scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap p2Scaled = m_view->playerPixmap(2).scaled(iconSize, iconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_p1Icon->setPixmap(p1Scaled);
    m_p2Icon->setPixmap(p2Scaled);
    m_currentPlayerIcon->setPixmap(m_game->currentPlayer() == 1 ? p1Scaled : p2Scaled);
    KDominateBoard::TileCount tc = m_game->countTiles();
    m_p1Count->setText(QString::number(tc.p1));
    m_p2Count->setText(QString::number(tc.p2));
    if (m_game->isGameOver()) {
        m_currentPlayerLabel->setText(m_game->winnerString());
        m_currentPlayerIcon->setVisible(false);
    } else {
        m_currentPlayerLabel->setText(i18n("Current player:"));
        m_currentPlayerIcon->setVisible(true);
    }
}

void KDominate::showTileCoords(int x, int y)
{
    if (x < 0 || y < 0) {
        statusBar()->showMessage(QString());
    } else {
        statusBar()->showMessage(QStringLiteral("(%1, %2)").arg(x).arg(y));
    }
}

void KDominate::statusMessage(const QString &message, bool timed)
{
    if (timed) {
        statusBar()->showMessage(message, kTimedStatusBarMessageDurationMillis);
    } else {
        statusBar()->showMessage(message);
    }
}

#include "moc_kdominate.cpp"
