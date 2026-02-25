/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QFile>
#include <QSignalSpy>
#include <QTest>

#include "kdominate_ai.h"
#include "kdominate_board.h"

class KDominateAiTest : public QObject
{
    Q_OBJECT

private:
    QStringList loadMapLines(const QString &filename);

    struct MoveResult {
        QPoint origin;
        QPoint dest;
    };
    MoveResult runAiMove(KDominateBoard &board, int player, int depth = 4, int timeout = 10000);
    bool playToEnd(KDominateBoard &board, int p1depth = 4, int p2depth = 4, int moveLimit = 50, int timeout = 10000);

private Q_SLOTS:
    void testWin1Move();
    void testLose1Move();
    void testJumpToWin();
    void testJumpToDraw();
    void testDoNotDelayDraw();
    void testDoNotDelayDrawObstacles();
    void testDoNotDelayLose();
    void testWinable();
};

QStringList KDominateAiTest::loadMapLines(const QString &filename)
{
    QFile file(QStringLiteral(MAPS_DIR) + QLatin1Char('/') + filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QString content = QString::fromUtf8(file.readAll());
    QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // Skip first line (display name)
    if (!lines.isEmpty()) {
        lines.removeFirst();
    }
    return lines;
}

KDominateAiTest::MoveResult KDominateAiTest::runAiMove(KDominateBoard &board, int player, int depth, int timeout)
{
    KDominateAi ai;
    ai.setDepth(depth);
    QSignalSpy spy(&ai, &KDominateAi::done);
    ai.getMove(board, player);
    spy.wait(timeout);
    if (spy.count() < 1) {
        return {QPoint(-1, -1), QPoint(-1, -1)};
    }
    QList<QVariant> args = spy.takeFirst();
    return {args.at(0).value<QPoint>(), args.at(1).value<QPoint>()};
}

bool KDominateAiTest::playToEnd(KDominateBoard &board, int p1depth, int p2depth, int moveLimit, int timeout)
{
    int moveCount = 0;
    while (!board.isGameOver() && moveCount < moveLimit) {
        int player = board.currentPlayer();
        int depth = player == 1 ? p1depth : p2depth;
        MoveResult result = runAiMove(board, player, depth, timeout);
        if (result.origin == QPoint(-1, -1)) {
            return false;
        }
        if (!board.move(result.origin, result.dest)) {
            return false;
        }
        moveCount++;
    }
    return board.isGameOver();
}

void KDominateAiTest::testWin1Move()
{
    QStringList lines = loadMapLines(QStringLiteral("win-1-move.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    MoveResult result = runAiMove(board, 1);
    QVERIFY2(result.origin != QPoint(-1, -1), "AI timed out");
    QCOMPARE(result.dest, QPoint(6, 3));

    QVERIFY(board.move(result.origin, result.dest));
    QVERIFY(board.isGameOver());
    QCOMPARE(board.winner(), 1);
}

void KDominateAiTest::testLose1Move()
{
    QStringList lines = loadMapLines(QStringLiteral("lose-1-move.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    MoveResult result = runAiMove(board, 1);
    QVERIFY2(result.origin != QPoint(-1, -1), "AI timed out");
    QCOMPARE(result.dest, QPoint(4, 1));

    QVERIFY(board.move(result.origin, result.dest));
    QVERIFY(board.isGameOver());
    QCOMPARE(board.winner(), 2);
}

void KDominateAiTest::testJumpToWin()
{
    QStringList lines = loadMapLines(QStringLiteral("jump-to-win.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 1);
}

void KDominateAiTest::testJumpToDraw()
{
    QStringList lines = loadMapLines(QStringLiteral("jump-to-draw.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 0);
}

void KDominateAiTest::testDoNotDelayDraw()
{
    QStringList lines = loadMapLines(QStringLiteral("do-not-delay-draw.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 0);
}

void KDominateAiTest::testDoNotDelayDrawObstacles()
{
    QStringList lines = loadMapLines(QStringLiteral("do-not-delay-draw-obstacles.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 0);
}

void KDominateAiTest::testDoNotDelayLose()
{
    QStringList lines = loadMapLines(QStringLiteral("do-not-delay-lose.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 1);
}

void KDominateAiTest::testWinable()
{
    QStringList lines = loadMapLines(QStringLiteral("winable.map"));
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    QVERIFY2(playToEnd(board, 6, 4), "Game did not complete within move limit");
    QCOMPARE(board.winner(), 1);
}

QTEST_GUILESS_MAIN(KDominateAiTest)

#include "kdominate_ai_test.moc"
