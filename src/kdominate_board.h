/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QPoint>
#include <QStringList>

#include <QList>
#include <QStack>

#include <optional>

class KDominateBoard
{
public:
    KDominateBoard();

    void newGame(int size);
    void loadFromMap(const QStringList &lines);

    std::pair<bool, bool> move(QPoint origin, QPoint dest);
    bool undo();

    struct TileCount {
        int p1, p2, empty, other;
    };
    TileCount countTiles() const;

    int currentPlayer() const
    {
        return m_currentPlayer;
    }
    int otherPlayer() const
    {
        const int other[3] = {0, 2, 1};
        return other[m_currentPlayer];
    }

    bool isWinner() const
    {
        return m_winner != -1;
    }
    int winner() const
    {
        return m_winner;
    }

    bool inBounds(QPoint p) const
    {
        return (p.x() >= 0 && p.y() >= 0 && p.x() < m_boardSize && p.y() < m_boardSize);
    }

    int size() const
    {
        return m_boardSize;
    }
    void setCurrentPlayer(int player)
    {
        m_currentPlayer = player;
    }

    const QList<int> &operator[](unsigned int a) const
    {
        return m_board[a];
    }

    int operator[](QPoint p) const
    {
        return m_board[p.x()][p.y()];
    }

    int at(QPoint p) const
    {
        return at(p.x(), p.y());
    }

    int at(int x, int y) const
    {
        if (!inBounds(QPoint(x, y)))
            return -1;
        return m_board[x][y];
    }

    void set(int x, int y, int value)
    {
        m_board[x][y] = value;
    }

    bool areMovementsAvailable(int player) const;
    std::optional<QPoint> fillNextEmpty(int player);
    bool determineWinner();

private:
    void expandConnected(QPoint tile);

    typedef QList<QList<int>> BoardMatrix;
    BoardMatrix m_board;

    int m_boardSize;
    int m_currentPlayer;
    int m_winner;

    struct PosAndPlayer {
        PosAndPlayer(QPoint p, int who)
            : pos(p)
            , player(who)
        {
        }
        QPoint pos;
        int player;
    };
    typedef QList<PosAndPlayer> UndoMovement;
    QStack<UndoMovement> m_undos;

    static const QPoint s_validMovementDirections[];
    static const int s_numDirections;
};
