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
    static QStringList loadMapLines(const QString &filename);

    struct MoveResult {
        QPoint origin;
        QPoint dest;
    };
    static MoveResult runAiMove(KDominateBoard &board, int player, int depth = 4, int timeout = 10000);

private Q_SLOTS:
    void testAi_data();
    void testAi();
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

void KDominateAiTest::testAi_data()
{
    QTest::addColumn<int>("expectedWinner");
    QTest::addColumn<int>("p1depth");
    QTest::addColumn<int>("p2depth");
    QTest::addColumn<int>("moveLimit");

    QTest::newRow("win-1-move")              << 1 << 4 << 4 << 1;
    QTest::newRow("lose-1-move")             << 2 << 4 << 4 << 1;
    QTest::newRow("jump-to-win")             << 1 << 4 << 4 << 5;
    QTest::newRow("jump-to-draw")            << 0 << 4 << 4 << 5;
    QTest::newRow("do-not-delay-draw")       << 0 << 4 << 4 << 5;
    QTest::newRow("do-not-delay-draw-walls") << 0 << 4 << 4 << 5;
    QTest::newRow("do-not-delay-lose")       << 1 << 4 << 4 << 5;
    QTest::newRow("hard-win")                << 1 << 6 << 4 << 20;
}

void KDominateAiTest::testAi()
{
    QFETCH(int, expectedWinner);
    QFETCH(int, p1depth);
    QFETCH(int, p2depth);
    QFETCH(int, moveLimit);

    QString mapFile = QString::fromLatin1(QTest::currentDataTag()) + QStringLiteral(".map");
    QStringList lines = loadMapLines(mapFile);
    QVERIFY(!lines.isEmpty());

    KDominateBoard board;
    board.loadFromMap(lines);

    int moveCount = 0;
    while (!board.isGameOver() && moveCount < moveLimit) {
        int player = board.currentPlayer();
        int depth = player == 1 ? p1depth : p2depth;
        MoveResult result = runAiMove(board, player, depth);
        QVERIFY2(result.origin != QPoint(-1, -1), "AI timeout");
        QVERIFY2(board.move(result.origin, result.dest), "AI returned an invalid move");
        moveCount++;
    }
    QVERIFY2(board.isGameOver(), "Game did not complete within move limit");
    QCOMPARE(board.winner(), expectedWinner);
}

QTEST_GUILESS_MAIN(KDominateAiTest)

#include "kdominate_ai_test.moc"
