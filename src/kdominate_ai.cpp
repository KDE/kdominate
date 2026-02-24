/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kdominate_ai.h"

#include "kdominate_board.h"
#include "kdominate_debug.h"

// Helper thread class
class AiThread : public QThread
{
    Q_OBJECT
public:
    AiThread(KDominateAi *ai)
        : m_ai(ai)
    {
    }
    void run() override
    {
        m_ai->computeMove();
    }

private:
    KDominateAi *m_ai;
};

const QList<QPoint> kCloneDirections = {
    QPoint(0, -1),
    QPoint(1, 0),
    QPoint(-1, 0),
    QPoint(0, 1),
    QPoint(1, 1),
    QPoint(1, -1),
    QPoint(-1, 1),
    QPoint(-1, -1),
};
const QList<QPoint> kJumpDirections = {
    QPoint(0, -2),
    QPoint(2, 0),
    QPoint(-2, 0),
    QPoint(0, 2),
};

KDominateAi::KDominateAi(QObject *parent)
    : QObject(parent)
    , m_depth(4)
    , m_stopped(false)
    , m_thread(nullptr)
    , m_workBoard(nullptr)
{
}

KDominateAi::~KDominateAi()
{
    if (m_thread) {
        m_stopped = true;
        m_thread->wait();
        delete m_thread;
    }
    delete m_workBoard;
}

void KDominateAi::setDepth(int depth)
{
    m_depth = depth;
}

void KDominateAi::stop()
{
    m_stopped = true;
    if (m_thread) {
        m_thread->wait();
    }
}

void KDominateAi::getMove(KDominateBoard &board, int player)
{
    m_stopped = false;

    // Create a working copy of the board for the AI thread
    delete m_workBoard;
    m_workBoard = new KDominateBoard();
    m_workBoard->newGame(board.size());
    for (int x = 0; x < board.size(); x++) {
        for (int y = 0; y < board.size(); y++) {
            m_workBoard->set(x, y, board[x][y]);
        }
    }
    m_workBoard->setCurrentPlayer(player);

    // Only needed for animation purposes, the AI doesn't need to update this
    m_workBoard->setSkipLastConvertedComputation(true);

    if (m_thread) {
        m_thread->wait();
        delete m_thread;
    }

    m_thread = new AiThread(this);
    // Use QueuedConnection so the signal crosses from the AI thread to the main thread
    connect(
        m_thread,
        &QThread::finished,
        this,
        [this]() {
            Q_EMIT done(m_resultOrigin, m_resultDest);
        },
        Qt::QueuedConnection);

    m_thread->start(QThread::IdlePriority);
}

void KDominateAi::computeMove()
{
    if (!m_workBoard) {
        m_resultOrigin = QPoint(-1, -1);
        m_resultDest = QPoint(-1, -1);
        return;
    }

    m_moveCount = 0;
    AiMove move = alphaBeta(*m_workBoard, m_workBoard->currentPlayer(), m_depth);
    qCDebug(KDOMINATE_LOG) << "AI explored" << m_moveCount << "moves";
    m_resultOrigin = move.origin;
    m_resultDest = move.dest;
}

int KDominateAi::staticEvaluationFunction(KDominateBoard &board, int initialPlayer, int depth) const
{
    int score = 0;

    if (board.isWinner()) {
        int winner = board.winner();
        if (initialPlayer == winner)
            score += 1000 + depth;
        else
            score -= (1000 - depth);
    }

    KDominateBoard::TileCount tc = board.countTiles();
    if (initialPlayer == 1)
        score += tc.p1 - tc.p2;
    else
        score += tc.p2 - tc.p1;

    return score;
}

KDominateAi::AiMove KDominateAi::alphaBeta(KDominateBoard &board, int initialPlayer, int depth, int alpha, int beta)
{
    AiMove bestAiMove;

    if (depth <= 0 || board.isWinner()) {
        bestAiMove.score = staticEvaluationFunction(board, initialPlayer, depth);
        return bestAiMove;
    }

    bool maximizing = initialPlayer == board.currentPlayer();
    bestAiMove.score = maximizing ? INT_MIN : INT_MAX;

    auto tryDirections = [&](const QList<QPoint> &directions) -> bool {
        for (int i = 0; i < board.size(); i++) {
            for (int j = 0; j < board.size(); j++) {
                QPoint origin(i, j);
                if (board[origin] != board.currentPlayer())
                    continue;

                for (QPoint p : directions) {
                    QPoint dest = origin + p;

                    auto [validMovement, jumpMovement] = board.move(origin, dest);
                    if (!validMovement)
                        continue;

                    m_moveCount++;
                    AiMove candidateAiMove = alphaBeta(board, initialPlayer, depth - 1, alpha, beta);

                    if ((maximizing && candidateAiMove.score > bestAiMove.score) || (!maximizing && candidateAiMove.score < bestAiMove.score)) {
                        bestAiMove.origin = origin;
                        bestAiMove.dest = dest;
                        bestAiMove.score = candidateAiMove.score;
                    }

                    board.undo();

                    if (maximizing)
                        alpha = qMax(alpha, bestAiMove.score);
                    else
                        beta = qMin(beta, bestAiMove.score);
                    if (beta <= alpha) {
                        return true; // pruned
                    }

                    if (m_stopped) {
                        return true; // stopped
                    }
                }
            }
        }
        return false;
    };

    // Try cloning actions before jumping actions since they tend to be better and help pruning
    if (!tryDirections(kCloneDirections)) {
        tryDirections(kJumpDirections);
    }

    if (bestAiMove.score == INT_MIN || bestAiMove.score == INT_MAX) {
        qCWarning(KDOMINATE_LOG) << "No moves found, THIS SHOULD NOT HAPPEN!";
        bestAiMove.score = staticEvaluationFunction(board, initialPlayer, depth);
    }

    return bestAiMove;
}

#include "kdominate_ai.moc"
#include "moc_kdominate_ai.cpp"
