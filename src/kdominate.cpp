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
#include "prefs.h"
#include "settingswidget.h"

#include <QFile>
#include <QTextStream>

#include <KActionCollection>
#include <KGameStandardAction>
#include <KLocalizedString>
#include <KStandardAction>
#include <QAction>
#include <QStatusBar>
#include <QWidgetAction>

#include "kdominate_debug.h"

const int kTimedStatusBarMessageDurationMillis = 2000;

KDominate::KDominate()
{
    // Determine initial board size by reading the saved map preset file.
    QString mapPath = Game::mapResourcePath(Prefs::mapIndex());
    KDominateBoard tmpBoard;
    QFile mapFile(mapPath);
    if (mapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&mapFile);
        QStringList lines;
        while (!in.atEnd())
            lines.append(in.readLine());
        mapFile.close();
        tmpBoard.loadFromMap(lines);
    }
    int initDim = tmpBoard.size();

    qCDebug(KDOMINATE_LOG) << "KDominate::KDominate() CONSTRUCTOR";
    m_view = new KBoardWidget(initDim, this);
    m_game = new Game(initDim, m_view, this);

    connect(m_game, &Game::playerChanged, this, &KDominate::changePlayerColor);
    connect(m_game, &Game::buttonChange, this, &KDominate::changeButton);
    connect(m_game, &Game::statusMessage, this, &KDominate::statusMessage);

    setCentralWidget(m_view);

    // init statusbar
    QString s = i18n("Current player:");
    statusBar()->addPermanentWidget(new QLabel(s));

    currentPlayer = new QLabel();
    currentPlayer->setFrameStyle(QFrame::NoFrame);
    changePlayerColor(1);
    statusBar()->addPermanentWidget(currentPlayer);

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
    }

    {
        QAction *action = KGameStandardAction::undo(
            this,
            [this]() {
                m_game->gameActions(Action::UNDO);
            },
            this);
        actionCollection()->addAction(action->objectName(), action);
        action->setEnabled(false);
    }

    {
        QAction *action = KGameStandardAction::redo(
            this,
            [this]() {
                m_game->gameActions(Action::REDO);
            },
            this);
        actionCollection()->addAction(action->objectName(), action);
        action->setEnabled(false);
    }

    {
        QAction *action = KGameStandardAction::quit(this, &KDominate::close, this);
        actionCollection()->addAction(action->objectName(), action);
    }

    {
        QAction *action = KStandardAction::preferences(
            this,
            [this]() {
                m_game->showSettingsDialog();
            },
            actionCollection());
        qCDebug(KDOMINATE_LOG) << "PREFERENCES ACTION is" << action->objectName();
        action->setIconText(i18n("Settings"));
    }

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

    setupGUI();

    connect(m_game, &Game::setAction, this, &KDominate::setAction);
    m_game->gameActions(Action::NEW);
}

void KDominate::changeButton(bool enabled, bool stop, const QString &caption)
{
    qCDebug(KDOMINATE_LOG) << "KDominate::changeButton (" << enabled << stop << caption << ")";
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

void KDominate::changePlayerColor(int newPlayer)
{
    // currentPlayer->setPixmap(m_view->playerPixmap(newPlayer));
    // currentPlayer->resize(30,30);
}

void KDominate::setAction(const Action a, const bool onOff)
{
    ((QAction *)actionCollection()->action(a))->setEnabled(onOff);
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
