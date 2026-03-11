/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QPoint>
#include <QThread>
#include <climits>

class KDominateBoard;

class AiThread;

class KDominateAi : public QObject
{
    Q_OBJECT
    friend class AiThread;

public:
    explicit KDominateAi(QObject *parent = nullptr);
    ~KDominateAi() override;

    int staticEvaluationFunction(KDominateBoard &board, int initialPlayer) const;

    void getMove(KDominateBoard &board, int player);
    void stop();
    void setDepth(int depth);

Q_SIGNALS:
    void done(QPoint origin, QPoint dest);

private:
    struct AiMove {
        QPoint origin;
        QPoint dest;
        int score = 0;
    };

    int m_depth;
    bool m_stopped;

    QThread *m_thread;

    // Working copy of the board for AI computation
    KDominateBoard *m_workBoard;

    // Results stored by the thread, emitted via signal after thread finishes
    QPoint m_resultOrigin;
    QPoint m_resultDest;

    AiMove alphaBeta(KDominateBoard &board, int maximizingPlayer, int depth, int alpha = INT_MIN, int beta = INT_MAX);

    int m_moveCount;

    void computeMove();
};
