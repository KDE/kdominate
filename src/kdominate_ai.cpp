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
    QPoint(-1, 0),
    QPoint(0, -1),
    QPoint(1, 0),
    QPoint(0, 1),
    QPoint(-1, -1),
    QPoint(-1, 1),
    QPoint(1, -1),
    QPoint(1, 1),
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
    int depth = m_depth;
    if (m_workBoard->countTiles().empty < 6) {
        depth += 6 - m_workBoard->countTiles().empty;
    }
    AiMove move = alphaBeta(*m_workBoard, m_workBoard->currentPlayer(), depth);

    qCDebug(KDOMINATE_LOG) << "AI explored" << m_moveCount << "moves, best move" << move.origin << "to" << move.dest << "with score" << move.score;
    m_resultOrigin = move.origin;
    m_resultDest = move.dest;
}

int KDominateAi::staticEvaluationFunction(KDominateBoard &board, int maximizingPlayer) const
{
    KDominateBoard::TileCount tc = board.countTiles();
    if (maximizingPlayer == 1)
        return tc.p1 - tc.p2;
    else
        return tc.p2 - tc.p1;
}

// Often two moves result in an identical board, this function deduplicates those to reduce the space we have to explore.
// Puts the cloning actions before jumping actions since they tend to be better and help pruning.
QList<std::pair<QPoint, QPoint>> computeMoves(KDominateBoard &board)
{
    int size = board.size();
    int currentPlayer = board.currentPlayer();
    QList<std::pair<QPoint, QPoint>> moves;
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            if (board[x][y] != 0) {
                continue;
            }
            for (const QPoint &p : kCloneDirections) {
                QPoint origin(x + p.x(), y + p.y());
                if (board.at(origin) == currentPlayer) {
                    moves.append(std::make_pair(origin, QPoint(x, y)));
                    break;
                }
            }
        }
    }
    // Cloning ensures the game will end, but jumping can delay the game forever. We could protect against stalemates by only
    // allowing jumps if the last N turns weren't jumps. KDominateBoard would neet to keep a count of jumps per player (maybe
    // resetting/decreasing it after a clone) and we would skip the next loop if too many jumps happened AND there are clone
    // moves available.  I didn't encounter any case of a stalemate in my tests, so I didn't implement that logic.
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            if (board[x][y] != 0) {
                continue;
            }
            for (const QPoint &p : kJumpDirections) {
                QPoint origin(x + p.x(), y + p.y());
                if (board.at(origin) == currentPlayer) {
                    moves.append(std::make_pair(origin, QPoint(x, y)));
                    break;
                }
            }
        }
    }
    return moves;
}

KDominateAi::AiMove KDominateAi::alphaBeta(KDominateBoard &board, int maximizingPlayer, int depth, int alpha, int beta)
{
    AiMove bestAiMove;

    int currentPlayer = board.currentPlayer();
    bool maximizing = (maximizingPlayer == currentPlayer);
    bestAiMove.score = maximizing ? INT_MIN : INT_MAX;

    if (board.isGameOver()) {
        int winner = board.winner();
        // all options equal, favor ending the game faster (at higher depth = less moves) when drawing/winning
        if (winner == 0) {
            bestAiMove.score = depth;
        } else if (winner == maximizingPlayer) {
            bestAiMove.score = 1000 + depth;
        } else {
            bestAiMove.score = -1000; // subtracting depth would unnecessarily delay losing
        }
        return bestAiMove;
    }

    if (depth <= 0) {
        bestAiMove.score = 10*staticEvaluationFunction(board, maximizingPlayer);
        return bestAiMove;
    }

    auto moves = computeMoves(board);

    for (const auto &[origin, dest] : moves) {
        QString indent;
        for (int i = 0; i < (4 - depth); i++) {
            indent += QStringLiteral("  ");
        }

        bool validMovement = board.move(origin, dest);
        if (!validMovement)
            continue;

        // qWarning().noquote() << indent << "Player" << currentPlayer << "move" << origin << "to" << dest;

        m_moveCount++;
        AiMove candidateAiMove = alphaBeta(board, maximizingPlayer, depth - 1, alpha, beta);

        if ((maximizing && candidateAiMove.score > bestAiMove.score) || (!maximizing && candidateAiMove.score < bestAiMove.score)) {
            bestAiMove.origin = origin;
            bestAiMove.dest = dest;
            bestAiMove.score = candidateAiMove.score;
            // qWarning().noquote() << indent << "New best score:" << bestAiMove.score << "isGameOver:" << board.isGameOver() << board.countTiles().empty;
        }

        board.undo();

        if (maximizing) {
            alpha = qMax(alpha, bestAiMove.score);
        } else {
            beta = qMin(beta, bestAiMove.score);
        }

        if (beta <= alpha) {
            return bestAiMove; // pruned
        }

        if (m_stopped) {
            return bestAiMove; // computation interrupted
        }
    }

    if (bestAiMove.score == INT_MIN || bestAiMove.score == INT_MAX) {
        qCWarning(KDOMINATE_LOG) << "No moves found, THIS SHOULD NOT HAPPEN!";
    }

    return bestAiMove;
}

#include "kdominate_ai.moc"
#include "moc_kdominate_ai.cpp"
