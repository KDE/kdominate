/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kdominate_board.h"

const QPoint KDominateBoard::s_validMovementDirections[] = {
    // Clone orthogonal (distance 1)
    QPoint(0, -1),
    QPoint(1, 0),
    QPoint(-1, 0),
    QPoint(0, 1),
    // Clone diagonal (distance 1)
    QPoint(1, 1),
    QPoint(1, -1),
    QPoint(-1, 1),
    QPoint(-1, -1),
    // Jump orthogonal (distance 2)
    QPoint(0, -2),
    QPoint(2, 0),
    QPoint(-2, 0),
    QPoint(0, 2)};
const int KDominateBoard::s_numDirections = 12;

KDominateBoard::KDominateBoard()
    : m_boardSize(0)
    , m_currentPlayer(1)
    , m_winner(-1)
{
}

void KDominateBoard::newGame(int size)
{
    m_boardSize = size;
    m_board = BoardMatrix(m_boardSize, QList<int>(m_boardSize, 0));
    m_board[0][0] = 1;
    m_board[m_boardSize - 1][m_boardSize - 1] = 1;
    m_board[0][m_boardSize - 1] = 2;
    m_board[m_boardSize - 1][0] = 2;

    m_currentPlayer = 1;
    m_winner = -1;
    m_undos.clear();
}

KDominateBoard::TileCount KDominateBoard::countTiles() const
{
    TileCount tc = {0, 0, 0, 0};
    for (int i = 0; i < m_boardSize; i++) {
        for (int j = 0; j < m_boardSize; j++) {
            if (m_board[i][j] == 0)
                tc.empty++;
            else if (m_board[i][j] == 1)
                tc.p1++;
            else if (m_board[i][j] == 2)
                tc.p2++;
            else
                tc.other++;
        }
    }
    return tc;
}

std::pair<bool, bool> KDominateBoard::move(QPoint origin, QPoint dest)
{
    if (at(origin) != m_currentPlayer)
        return std::make_pair(false, false);
    if (at(dest) != 0)
        return std::make_pair(false, false);

    bool validMovement = false;
    bool jumpMovement = false;

    int diffx = qAbs(origin.x() - dest.x());
    int diffy = qAbs(origin.y() - dest.y());

    if ((diffx + diffy == 1) || (diffx == 1 && diffy == 1)) {
        validMovement = true;
        jumpMovement = false;
    } else if ((diffx == 0 && diffy == 2) || (diffx == 2 && diffy == 0)) {
        // can't jump over wall
        validMovement = at((origin.x() + dest.x()) / 2, (origin.y() + dest.y()) / 2) != -1;
        jumpMovement = true;
    }

    if (validMovement) {
        m_undos.push(UndoMovement());
        UndoMovement &umovs = m_undos.top();
        umovs.append(PosAndPlayer(dest, 0));
        int player = m_board[origin.x()][origin.y()];
        m_board[dest.x()][dest.y()] = player;
        if (jumpMovement) {
            umovs.append(PosAndPlayer(origin, player));
            m_board[origin.x()][origin.y()] = 0;
        }
        expandConnected(dest);
        if (!determineWinner()) {
            m_currentPlayer = otherPlayer();
        }
    }

    return std::make_pair(validMovement, jumpMovement);
}

bool KDominateBoard::undo()
{
    if (m_undos.empty()) {
        return false;
    }

    m_winner = -1;

    UndoMovement um = m_undos.pop();
    QPoint aux = um[0].pos;
    m_currentPlayer = m_board[aux.x()][aux.y()];
    for (int i = 0; i < um.size(); i++) {
        QPoint p = um[i].pos;
        int player = um[i].player;
        m_board[p.x()][p.y()] = player;
    }

    return true;
}

void KDominateBoard::expandConnected(QPoint tile)
{
    int player = m_board[tile.x()][tile.y()];
    int other = otherPlayer();

    UndoMovement &umovs = m_undos.top();

    // 8-way capture: orthogonal + diagonal neighbors
    static const QPoint neighbors[] = {QPoint(0, 1), QPoint(0, -1), QPoint(-1, 0), QPoint(1, 0), QPoint(1, 1), QPoint(1, -1), QPoint(-1, 1), QPoint(-1, -1)};

    for (const QPoint &dir : neighbors) {
        QPoint nb = tile + dir;
        if (at(nb.x(), nb.y()) == other) {
            umovs.append(PosAndPlayer(nb, m_board[nb.x()][nb.y()]));
            m_board[nb.x()][nb.y()] = player;
        }
    }
}

void KDominateBoard::loadFromMap(const QStringList &lines)
{
    // Determine board size from map lines
    int rows = lines.size();
    int cols = 0;
    for (const QString &line : lines) {
        if (line.length() > cols) {
            cols = line.length();
        }
    }
    m_boardSize = qMax(rows, cols);

    m_board = BoardMatrix(m_boardSize, QList<int>(m_boardSize, 0));
    m_currentPlayer = 1;
    m_winner = -1;
    m_undos.clear();

    for (int y = 0; y < rows && y < m_boardSize; y++) {
        const QString &line = lines.at(y);
        for (int x = 0; x < line.length() && x < m_boardSize; x++) {
            QChar ch = line.at(x);
            if (ch == QLatin1Char('1')) {
                m_board[x][y] = 1;
            } else if (ch == QLatin1Char('2')) {
                m_board[x][y] = 2;
            } else if (ch == QLatin1Char('X') || ch == QLatin1Char('x')) {
                m_board[x][y] = -1;
            }
            // '.' or anything else = 0 (empty)
        }
    }
}

std::optional<QPoint> KDominateBoard::fillNextEmpty(int player)
{
    UndoMovement &umovs = m_undos.top();
    for (int y = 0; y < m_boardSize; y++) {
        for (int x = 0; x < m_boardSize; x++) {
            if (m_board[x][y] == 0) {
                QPoint pos(x, y);
                umovs.append(PosAndPlayer(pos, 0));
                m_board[x][y] = player;
                return pos;
            }
        }
    }
    determineWinner();
    return std::nullopt;
}

bool KDominateBoard::determineWinner()
{
    TileCount tc = countTiles();
    if (tc.empty == 0 || tc.p1 == 0 || tc.p2 == 0) {
        if (tc.p1 > tc.p2) {
            m_winner = 1;
        } else if (tc.p1 < tc.p2) {
            m_winner = 2;
        } else {
            m_winner = 0;
        }
        return true;
    }
    return false;
}

bool KDominateBoard::areMovementsAvailable(int player) const
{
    for (int i = 0; i < m_boardSize; i++) {
        for (int j = 0; j < m_boardSize; j++) {
            if (m_board[i][j] != player)
                continue;
            if (player == 0)
                return true;
            for (int k = 0; k < s_numDirections; k++) {
                QPoint dest = QPoint(i, j) + s_validMovementDirections[k];
                if (at(dest) != 0)
                    continue;
                // can't jump over wall
                bool isJump = qMax(qAbs(s_validMovementDirections[k].x()), qAbs(s_validMovementDirections[k].y())) > 1;
                if (isJump && at((i + dest.x()) / 2, (j + dest.y()) / 2) == -1)
                    continue;
                return true;
            }
        }
    }
    return false;
}
