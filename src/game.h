/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 * SPDX-FileCopyrightText: 2012-2013 Ian Wadham <iandw.au@gmail.com>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QStringList>

#include "ktilewidget.h"

class KBoardWidget;
class SettingsWidget;
class KDominateBoard;
class KDominateAi;

enum Action {
    NEW,
    HINT,
    BUTTON,
    UNDO,
    REDO,
};

class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game(const int dim, KBoardWidget *view, QWidget *parent = nullptr);
    ~Game() override;

    void showSettingsDialog();
    void shutdown();

    static QStringList availableMapFiles(); // Returns sorted list of map filenames from :/maps/ resource directory
    static QString mapResourcePath(int mapIndex); // Returns resource path for a given map index (e.g. ":/maps/arena.map")
    static QString mapDisplayName(const QString &resourcePath); // Reads a map file's display name from its header comment

public Q_SLOTS:
    void gameActions(int action);

Q_SIGNALS:
    void playerChanged(int newPlayer);
    void buttonChange(bool enabled, bool stop = false, const QString &caption = QString());
    void setAction(const Action a, const bool onOff);
    void statusMessage(const QString &message, bool timed);

private:
    void showWinner();
    void setUpNextTurn();
    void computeMove();
    void showingDone(QPoint origin, QPoint dest);
    void doMove(QPoint origin, QPoint dest);
    void moveDone();
    void buttonClick();
    void undoClick();
    void redoClick();
    void setStopAction();
    void loadImmediateSettings();
    void loadPlayerSettings();
    bool undo();
    bool redo();
    bool newGameOK();
    void reset();
    virtual void setSize(int dim);
    bool isComputer(int player) const;
    void updateAllTiles();
    void saveSnapshot(QPoint origin, QPoint dest);
    void highlightValidDestinations(QPoint origin);

private Q_SLOTS:
    void newGame();
    void newSettings();
    void startHumanMove(int x, int y);
    void moveCalculationDone(QPoint origin, QPoint dest);
    void animationDone(int index);

private:
    enum class GameState {
        Idle, // No game active or game over
        HumanTurn, // Human player's turn, board clicks enabled
        WaitingForButton, // Computer's turn, waiting for button press
        Computing, // AI thread running
        Stopping, // AI being stopped, will become WaitingForButton
        ShowingMove, // Blink animation playing
        Aborting // Shutdown in progress
    };

    GameState m_state;
    bool isBusy() const;
    int m_moveNo;
    bool m_interrupting;

    QWidget *m_parent;
    KBoardWidget *m_view;
    SettingsWidget *m_settingsPage;

    KDominateBoard *m_board;
    KDominateAi *m_ai;
    int m_size;
    int m_currentPlayer;

    // Two-click move state
    bool m_selected;
    QPoint m_selectedOrigin;

    // Move being shown via blink animation (AI move or hint)
    QPoint m_pendingMoveOrigin;
    QPoint m_pendingMoveDest;

    bool computerPlOne;
    bool computerPlTwo;
    bool m_pauseForComputer;

    // Undo/redo: stores only move coordinates (board tracks its own undo state)
    struct MoveRecord {
        QPoint origin;
        QPoint dest;
    };
    QList<MoveRecord> m_undoList;
    int m_undoIndex;
    int m_mapIndex = 0;
};
