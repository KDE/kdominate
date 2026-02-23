/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 * SPDX-FileCopyrightText: 2012-2013 Ian Wadham <iandw.au@gmail.com>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "game.h"
#include "kboardwidget.h"
#include "kdominate_ai.h"
#include "kdominate_board.h"
#include "kdominate_version.h"
#include "settingswidget.h"

#include "kdominate_debug.h"
#include <QFileDialog>
#include <QTemporaryFile>

#include <KConfigDialog>
#include <KIO/FileCopyJob>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>

#include "prefs.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

QStringList Game::availableMapFiles()
{
    QDir dir(QStringLiteral(":/maps"));
    QStringList mapFiles = dir.entryList({QStringLiteral("*.map")}, QDir::Files, QDir::Name);
    mapFiles.sort();
    return mapFiles;
}

QString Game::mapResourcePath(int mapIndex)
{
    QStringList files = availableMapFiles();
    if (mapIndex < 0 || mapIndex >= files.size())
        mapIndex = 0;
    return QStringLiteral(":/maps/") + files.at(mapIndex);
}

QString Game::mapDisplayName(const QString &resourcePath)
{
    QFile f(resourcePath);
    Q_UNUSED(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QString englishName = QTextStream(&f).readLine();
    // Names should have translations since they are extracted in Messages.sh
    return ki18n(englishName.toUtf8().constData()).toString();
}

KDominateBoard::TileCount Game::countTiles() const
{
    return m_board->countTiles();
}

Game::Game(const int d, KBoardWidget *view, QWidget *parent)
    : QObject((QObject *)parent)
    , m_state(GameState::Idle)
    , m_moveNo(0)
    , m_interrupting(false)
    , m_parent(parent)
    , m_view(view)
    , m_settingsPage(nullptr)
    , m_size(d)
    , m_currentPlayer(1)
    , m_selected(false)
    , computerPlOne(false)
    , computerPlTwo(false)
    , m_pauseForComputer(false)
    , m_undoIndex(0)
{
    qCDebug(KDOMINATE_LOG) << "CONSTRUCT Game: size" << m_size;
    m_board = new KDominateBoard();
    m_ai = new KDominateAi(this);

    m_autoFillTimer = new QTimer(this);
    m_autoFillTimer->setInterval(150);
    connect(m_autoFillTimer, &QTimer::timeout, this, &Game::autoFillNextTile);

    connect(m_view, &KBoardWidget::mouseClick, this, &Game::startHumanMove);
    connect(m_ai, &KDominateAi::done, this, &Game::moveCalculationDone);
    connect(m_view, &KBoardWidget::animationDone, this, &Game::animationDone);
}

Game::~Game()
{
    if ((m_state != GameState::Idle) && (m_state != GameState::Aborting)) {
        shutdown();
    }
    delete m_board;
}

void Game::gameActions(const int action)
{
    qCDebug(KDOMINATE_LOG) << "GAME ACTION IS" << action;
    if (isBusy() && (action != BUTTON) && (action != Action::NEW)) {
        m_view->showPopup(i18n("Sorry, doing a move..."));
        return;
    }

    switch (action) {
    case Action::NEW:
        newGame();
        break;
    case Action::HINT:
        if (!m_board->isWinner() && !isComputer(m_currentPlayer)) {
            KTileWidget::enableClicks(false);
            computeMove();
        }
        break;
    case Action::BUTTON:
        buttonClick();
        break;
    case Action::UNDO:
        undoClick();
        break;
    case Action::REDO:
        redoClick();
        break;
    default:
        break;
    }
}

void Game::showWinner()
{
    Q_EMIT buttonChange(false, false, i18n("Game over"));
    KTileWidget::enableClicks(false);
    KDominateBoard::TileCount tc = m_board->countTiles();
    QString s;
    int w = m_board->winner();
    if (w == 0) {
        s = i18n("Draw! (%1-%2)", tc.p1, tc.p2);
    } else {
        s = i18n("Player %1 wins! (%2-%3)", w, tc.p1, tc.p2);
    }
    Q_EMIT statusMessage(s, false);
    KMessageBox::information(m_view, s, i18n("Game Over"));
}

void Game::showSettingsDialog()
{
    KConfigDialog *settings = KConfigDialog::exists(QStringLiteral("settings"));
    if (!settings) {
        settings = new KConfigDialog(m_parent, QStringLiteral("settings"), Prefs::self());
        settings->setFaceType(KPageDialog::Plain);
        SettingsWidget *widget = new SettingsWidget(m_parent);
        settings->addPage(widget, i18n("General"), QStringLiteral("games-config-options"));
        connect(settings, &KConfigDialog::settingsChanged, this, &Game::newSettings);
        m_settingsPage = widget;
    }
    settings->show();
    settings->raise();
}

void Game::newSettings()
{
    qCDebug(KDOMINATE_LOG) << "NEW SETTINGS" << "m_state" << int(m_state) << "mapIndex:" << Prefs::mapIndex() << "m_size" << m_size;
    loadImmediateSettings();

    if (Prefs::mapIndex() != m_mapIndex) {
        QMetaObject::invokeMethod(this, &Game::newGame, Qt::QueuedConnection);
        return;
    } else if (m_state == GameState::HumanTurn || m_state == GameState::WaitingForButton) {
        loadPlayerSettings();
        setUpNextTurn();
    }
}

void Game::loadImmediateSettings()
{
    qCDebug(KDOMINATE_LOG) << "GAME LOAD IMMEDIATE SETTINGS entered";

    bool reColorTiles = m_view->loadSettings();
    if (reColorTiles) {
        Q_EMIT playerChanged(m_currentPlayer);
    }
}

void Game::loadPlayerSettings()
{
    qCDebug(KDOMINATE_LOG) << "GAME LOAD PLAYER SETTINGS entered";
    bool oldComputerPlayer = isComputer(m_currentPlayer);

    m_pauseForComputer = Prefs::pauseForComputer();
    computerPlOne = Prefs::computerPlayer1();
    computerPlTwo = Prefs::computerPlayer2();

    qCDebug(KDOMINATE_LOG) << "AI 1" << computerPlOne << "AI 2" << computerPlTwo << "m_pauseForComputer" << m_pauseForComputer;

    if (isComputer(m_currentPlayer) && (!oldComputerPlayer)) {
        qCDebug(KDOMINATE_LOG) << "New computer player set: must wait.";
        m_state = GameState::WaitingForButton;
    }
}

void Game::startHumanMove(int x, int y)
{
    qCDebug(KDOMINATE_LOG) << "CLICK" << x << y;

    bool humanPlayer = (!isComputer(m_currentPlayer));
    if (!humanPlayer && m_state != GameState::WaitingForButton) {
        return;
    }
    if (m_state == GameState::WaitingForButton) {
        buttonClick();
        return;
    }

    int cellOwner = m_board->at(x, y);

    if (!m_selected) {
        // First click: select a source tile
        if (cellOwner == m_currentPlayer) {
            m_selected = true;
            m_selectedOrigin = QPoint(x, y);
            m_view->selectTile(m_selectedOrigin);
            highlightValidDestinations(m_selectedOrigin);
            Q_EMIT statusMessage(i18n("Select destination"), false);
        }
    } else {
        // Second click: select destination or change selection
        if (cellOwner == m_currentPlayer) {
            if (m_selectedOrigin == QPoint(x, y)) {
                // Clicked same tile: deselect
                m_selected = false;
                m_view->clearValidMoveHighlights();
                m_view->deselectAll();
                Q_EMIT statusMessage(QString(), false);
            } else {
                // Clicked different own tile: change selection
                m_selectedOrigin = QPoint(x, y);
                m_view->selectTile(m_selectedOrigin);
                highlightValidDestinations(m_selectedOrigin);
                Q_EMIT statusMessage(i18n("Select destination"), false);
            }
        } else if (cellOwner == 0) {
            // Clicked empty tile: attempt move
            QPoint dest(x, y);
            m_view->clearValidMoveHighlights();
            m_view->deselectAll();
            m_selected = false;

            // Validate move (distance check)
            int diffx = qAbs(m_selectedOrigin.x() - dest.x());
            int diffy = qAbs(m_selectedOrigin.y() - dest.y());
            bool validDist = ((diffx + diffy == 1) || (diffx == 1 && diffy == 1) || (diffx == 0 && diffy == 2) || (diffx == 2 && diffy == 0));

            if (validDist) {
                m_state = GameState::Idle;
                m_moveNo++;
                KTileWidget::enableClicks(false);
                doMove(m_selectedOrigin, dest);
                Q_EMIT statusMessage(QString(), false);
            } else {
                Q_EMIT statusMessage(i18n("Invalid move - too far"), true);
            }
        } else {
            // Clicked opponent tile or invalid: deselect
            m_selected = false;
            m_view->clearValidMoveHighlights();
            m_view->deselectAll();
            Q_EMIT statusMessage(QString(), false);
        }
    }
}

void Game::setUpNextTurn()
{
    // Deselect any selection and clear highlights
    m_selected = false;
    m_view->clearValidMoveHighlights();
    m_view->deselectAll();

    qCDebug(KDOMINATE_LOG) << "setUpNextTurn" << m_currentPlayer << computerPlOne << computerPlTwo << "pause:" << m_pauseForComputer
                           << "state:" << int(m_state);

    if (isComputer(m_currentPlayer)) {
        if (m_pauseForComputer || m_interrupting || (m_moveNo == 0)) {
            m_interrupting = false;
            m_state = GameState::WaitingForButton;
            if (computerPlOne && computerPlTwo) {
                if (m_moveNo == 0) {
                    Q_EMIT buttonChange(true, false, i18n("Start game"));
                } else {
                    Q_EMIT buttonChange(true, false, i18n("Continue game"));
                }
            } else {
                Q_EMIT buttonChange(true, false, i18n("Start computer move"));
            }
            qCDebug(KDOMINATE_LOG) << "COMPUTER MUST WAIT";
            KTileWidget::enableClicks(true);
            return;
        }
        qCDebug(KDOMINATE_LOG) << "COMPUTER MUST MOVE";
        m_state = GameState::Computing;
        KTileWidget::enableClicks(false);
        computeMove();
    } else {
        qCDebug(KDOMINATE_LOG) << "HUMAN TO MOVE";
        m_state = GameState::HumanTurn;
        KTileWidget::enableClicks(true);
        if (computerPlOne || computerPlTwo) {
            Q_EMIT buttonChange(false, false, i18n("Your turn"));
        } else {
            Q_EMIT buttonChange(false, false, i18n("Player %1", m_currentPlayer));
        }
    }
}

void Game::computeMove()
{
    m_view->setWaitCursor();
    m_state = GameState::Computing;
    setStopAction();
    Q_EMIT setAction(Action::HINT, false);
    if (isComputer(m_currentPlayer)) {
        Q_EMIT statusMessage(i18n("Computer player %1 is moving", m_currentPlayer), false);
    }
    m_ai->getMove(*m_board, m_currentPlayer);
}

void Game::moveCalculationDone(QPoint origin, QPoint dest)
{
    if (m_state == GameState::Aborting) {
        m_state = GameState::Idle;
        return;
    }
    if (m_state == GameState::Stopping) {
        m_view->setNormalCursor();
        m_view->hidePopup();
        m_interrupting = false;
        m_state = GameState::WaitingForButton;
        if (computerPlOne && computerPlTwo) {
            Q_EMIT buttonChange(true, false, i18n("Continue game"));
        } else {
            Q_EMIT buttonChange(true, false, i18n("Start computer move"));
        }
        Q_EMIT setAction(Action::HINT, true);
        KTileWidget::enableClicks(true);
        return;
    }

    m_state = GameState::Idle;

    if (origin.x() < 0 || origin.y() < 0 || dest.x() < 0 || dest.y() < 0) {
        m_view->setNormalCursor();
        KMessageBox::error(m_view, i18n("The computer could not find a valid move."));
        return;
    }

    // Blink both origin and destination tiles to show the move
    m_pendingMoveOrigin = origin;
    m_pendingMoveDest = dest;
    int originIdx = origin.x() * m_size + origin.y();
    int destIdx = dest.x() * m_size + dest.y();
    m_view->startComputerMoveAnimation(originIdx, destIdx);
    m_state = GameState::ShowingMove;
    setStopAction();
}

void Game::showingDone(QPoint origin, QPoint dest)
{
    if (isComputer(m_currentPlayer)) {
        m_moveNo++;
        doMove(origin, dest);
    } else {
        // Finish Hint animation
        moveDone();
        // Highlight both source and destination briefly for hints
        m_view->timedTileHighlight(origin.x() * m_size + origin.y());
        Q_EMIT statusMessage(i18n("Hint: move from (%1,%2) to (%3,%4)", origin.x() + 1, origin.y() + 1, dest.x() + 1, dest.y() + 1), false);
        setUpNextTurn();
    }
}

Owner Game::boardCellToOwner(int cell)
{
    if (cell == 1) {
        return Owner::One;
    } else if (cell == 2) {
        return Owner::Two;
    } else if (cell == -1) {
        return Owner::Wall;
    } else {
        return Owner::Nobody;
    }
}

void Game::doMove(QPoint origin, QPoint dest)
{
    // Save snapshot for undo
    saveSnapshot(origin, dest);

    Q_EMIT setAction(Action::UNDO, true);
    Q_EMIT setAction(Action::REDO, false);

    // Execute the move on the board
    auto [validMovement, jumpMovement] = m_board->move(origin, dest);
    if (!validMovement) {
        qCDebug(KDOMINATE_LOG) << "INVALID MOVE attempted:" << origin << "->" << dest;
        moveDone();
        setUpNextTurn();
        return;
    }

    Owner newOwner = updateTile(dest);

    // Build converted tile index list but don't update the displayed tiles yet, it is animated later.
    const QList<QPoint> &converted = m_board->lastConvertedTiles();
    QList<int> indices;
    for (const QPoint &p : converted) {
        indices.append(p.x() * m_size + p.y());
    }

    int destIdx = dest.x() * m_size + dest.y();
    int originIdx = jumpMovement ? (origin.x() * m_size + origin.y()) : -1;

    m_view->startMoveAnimation(destIdx, originIdx, indices, newOwner);
    m_state = GameState::AnimatingConversion;
}

void Game::finishMove()
{
    // Update all tile displays
    updateAllTiles();

    // Check for winner
    if (m_board->isWinner()) {
        moveDone();
        showWinner();
        return;
    }

    // Change player (the board already changed currentPlayer internally)
    m_currentPlayer = m_board->currentPlayer();
    if (!m_board->areMovementsAvailable(m_currentPlayer) && !m_board->isWinner()) {
        // Opponent can't move: auto-fill all empty cells for the other player
        m_autoFillPlayer = (m_currentPlayer == 1) ? 2 : 1;
        m_state = GameState::AutoFilling;
        Q_EMIT setAction(Action::UNDO, false);
        Q_EMIT buttonChange(true, true, i18n("Skip"));
        m_autoFillTimer->start();
        return;
    }
    moveDone();
    Q_EMIT playerChanged(m_currentPlayer);
    setUpNextTurn();
}

void Game::moveDone()
{
    m_view->setNormalCursor();
    m_state = GameState::Idle;
    Q_EMIT setAction(Action::HINT, true);
    m_view->hidePopup();
}

void Game::buttonClick()
{
    qCDebug(KDOMINATE_LOG) << "BUTTON CLICK seen: m_state:" << int(m_state);
    if (m_state == GameState::AutoFilling) {
        finishAutoFill();
        return;
    }
    if (m_board->isWinner()) {
        KTileWidget::enableClicks(false);
        return;
    }
    if (m_state == GameState::WaitingForButton) {
        computeMove();
    } else if (m_state == GameState::Computing) {
        if (!m_pauseForComputer && !m_interrupting && computerPlOne && computerPlTwo) {
            m_interrupting = true;
            m_view->showPopup(i18n("Finishing move..."));
            setStopAction();
        } else {
            m_ai->stop();
            m_state = GameState::Stopping;
        }
    } else if (m_state == GameState::ShowingMove) {
        m_view->killAnimation();
        showingDone(m_pendingMoveOrigin, m_pendingMoveDest);
    } else if (m_state == GameState::AnimatingConversion) {
        m_view->killAnimation();
        finishMove();
    }
}

void Game::setStopAction()
{
    if ((!m_pauseForComputer) && (!m_interrupting) && (computerPlOne && computerPlTwo)) {
        if (m_state == GameState::Computing) {
            Q_EMIT buttonChange(true, true, i18n("Interrupt game"));
        } else if (m_state == GameState::ShowingMove) {
            Q_EMIT buttonChange(true, true, i18n("Stop showing move"));
        }
        return;
    }
    if (m_state == GameState::Computing) {
        Q_EMIT buttonChange(true, true, i18n("Stop computing"));
    } else if (m_state == GameState::ShowingMove) {
        Q_EMIT buttonChange(true, true, i18n("Stop showing move"));
    }
}

void Game::newGame()
{
    qCDebug(KDOMINATE_LOG) << "NEW GAME entered: state" << int(m_state) << "won?" << (m_board->isWinner());
    if (newGameOK()) {
        qCDebug(KDOMINATE_LOG) << "QDEBUG: newGameOK() =" << true;
        shutdown();
        m_view->setNormalCursor();
        m_view->hidePopup();
        loadImmediateSettings();
        loadPlayerSettings();
        qCDebug(KDOMINATE_LOG) << "Entering reset();";
        reset();
        Q_EMIT setAction(Action::UNDO, false);
        Q_EMIT setAction(Action::REDO, false);
        Q_EMIT statusMessage(i18n("New Game"), false);
        m_moveNo = 0;
        setUpNextTurn();
    }
}

void Game::undoClick()
{
    bool moreToUndo = undo();
    Q_EMIT setAction(Action::UNDO, moreToUndo);
    Q_EMIT setAction(Action::REDO, true);
}

void Game::redoClick()
{
    bool moreToRedo = redo();
    Q_EMIT setAction(Action::REDO, moreToRedo);
    Q_EMIT setAction(Action::UNDO, true);
}

bool Game::newGameOK()
{
    if ((m_moveNo == 0) || m_board->isWinner()) {
        return true;
    }

    QString query = i18n(
        "You have requested a new game, but "
        "there is already a game in progress.\n\n"
        "Do you wish to abandon the current game?");
    int reply = KMessageBox::questionTwoActions(m_view, query, i18n("New Game?"), KGuiItem(i18n("Abandon Game")), KGuiItem(i18n("Continue Game")));
    if (reply == KMessageBox::PrimaryAction) {
        return true;
    }
    // Restore the map index if user cancelled
    if (Prefs::mapIndex() != m_mapIndex) {
        Prefs::setMapIndex(m_mapIndex);
        if (m_settingsPage) {
            m_settingsPage->kcfg_MapIndex->setCurrentIndex(m_mapIndex);
        }
        Prefs::self()->save();
    }
    return false;
}

void Game::reset()
{
    m_view->reset();

    m_mapIndex = Prefs::mapIndex();
    QString path = mapResourcePath(m_mapIndex);
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        QStringList lines;
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        f.close();
        lines.removeFirst(); // Remove the map name
        m_board->loadFromMap(lines);
        int newSize = m_board->size();
        if (newSize != m_size) {
            m_size = newSize;
            m_view->setSize(m_size);
        }
    }

    m_currentPlayer = 1;
    m_selected = false;

    m_state = GameState::Idle;

    // Clear undo history
    m_undoList.clear();
    m_undoIndex = 0;

    // Update the display to show initial board state
    updateAllTiles();

    Q_EMIT playerChanged(m_currentPlayer);
}

bool Game::undo()
{
    if (m_undoIndex <= 0) {
        return false;
    }
    m_undoIndex--;
    MoveRecord &snap = m_undoList[m_undoIndex];

    m_board->undo();
    m_currentPlayer = m_board->currentPlayer();
    m_moveNo--;

    updateAllTiles();

    // Highlight the move that was undone
    int originIdx = snap.origin.x() * m_size + snap.origin.y();
    m_view->timedTileHighlight(originIdx);

    Q_EMIT playerChanged(m_currentPlayer);

    m_interrupting = isComputer(m_currentPlayer);
    m_state = GameState::Idle;
    setUpNextTurn();

    return (m_undoIndex > 0);
}
bool Game::redo()
{
    if (m_undoIndex >= m_undoList.size()) {
        return false;
    }
    MoveRecord &snap = m_undoList[m_undoIndex];

    m_board->move(snap.origin, snap.dest);
    m_undoIndex++;
    m_moveNo++;

    m_currentPlayer = m_board->currentPlayer();
    if (!m_board->areMovementsAvailable(m_currentPlayer) && !m_board->isWinner()) {
        int fillingPlayer = (m_currentPlayer == 1) ? 2 : 1;
        while (m_board->fillNextEmpty(fillingPlayer)) { }
    }
    updateAllTiles();

    int destIdx = snap.dest.x() * m_size + snap.dest.y();
    m_view->timedTileHighlight(destIdx);

    Q_EMIT playerChanged(m_currentPlayer);

    if (m_board->isWinner()) {
        showWinner();
        return false;
    }

    m_interrupting = isComputer(m_currentPlayer);
    m_state = GameState::Idle;
    setUpNextTurn();
    return (m_undoIndex < m_undoList.size());
}

void Game::setSize(int d)
{
    if (d != m_size) {
        shutdown();
        m_size = d;
        m_view->setSize(d);
    }
}

void Game::shutdown()
{
    m_view->killAnimation();
    m_autoFillTimer->stop();
    if (m_state == GameState::Computing) {
        m_ai->stop();
        m_state = GameState::Aborting;
    } else if (m_state == GameState::Stopping) {
        m_state = GameState::Aborting;
    } else if (m_state != GameState::Aborting) {
        m_state = GameState::Idle;
    }
}

bool Game::isComputer(int player) const
{
    return player == 1 ? computerPlOne : computerPlTwo;
}

void Game::autoFillNextTile()
{
    auto pos = m_board->fillNextEmpty(m_autoFillPlayer);
    if (!pos) {
        finishAutoFill();
        return;
    }
    Owner owner = (m_autoFillPlayer == 1) ? Owner::One : Owner::Two;
    m_view->displayTile(*pos, owner);
}

void Game::finishAutoFill()
{
    m_autoFillTimer->stop();
    // Fill any remaining empty cells instantly (in case of skip)
    while (m_board->fillNextEmpty(m_autoFillPlayer)) { }
    updateAllTiles();
    moveDone();
    showWinner();
    Q_EMIT setAction(Action::UNDO, true);
}

void Game::animationDone(int index)
{
    Q_UNUSED(index);
    if (m_state == GameState::ShowingMove) {
        showingDone(m_pendingMoveOrigin, m_pendingMoveDest);
    } else if (m_state == GameState::AnimatingConversion) {
        finishMove();
    }
}

Owner Game::updateTile(QPoint p)
{
    int cell = m_board->at(p);
    Owner owner = boardCellToOwner(cell);
    m_view->displayTile(p, owner);
    return owner;
}

void Game::updateAllTiles()
{
    for (int x = 0; x < m_size; x++) {
        for (int y = 0; y < m_size; y++) {
            updateTile(QPoint(x, y));
        }
    }
}

void Game::highlightValidDestinations(QPoint origin)
{
    // Clone directions: 4 orthogonal + 4 diagonal (distance 1)
    static const QPoint cloneDirs[] = {QPoint(0, -1), QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(1, 1), QPoint(1, -1), QPoint(-1, 1), QPoint(-1, -1)};
    // Jump directions: 4 orthogonal (distance 2)
    static const QPoint jumpDirs[] = {QPoint(0, -2), QPoint(2, 0), QPoint(-2, 0), QPoint(0, 2)};

    QList<int> cloneIndices;
    QList<int> jumpIndices;
    for (const QPoint &d : cloneDirs) {
        QPoint dest = origin + d;
        if (m_board->inBounds(dest) && m_board->at(dest) == 0) {
            cloneIndices.append(dest.x() * m_size + dest.y());
        }
    }
    for (const QPoint &d : jumpDirs) {
        QPoint dest = origin + d;
        if (m_board->inBounds(dest) && m_board->at(dest) == 0 && m_board->at(origin + d / 2) != -1) {
            jumpIndices.append(dest.x() * m_size + dest.y());
        }
    }
    m_view->highlightValidMoves(m_currentPlayer, cloneIndices, jumpIndices);
}

void Game::saveSnapshot(QPoint origin, QPoint dest)
{
    // Remove any redo entries beyond the current position
    while (m_undoList.size() > m_undoIndex) {
        m_undoList.removeLast();
    }

    MoveRecord snap;
    snap.origin = origin;
    snap.dest = dest;

    m_undoList.append(snap);
    m_undoIndex++;
}

bool Game::isBusy() const
{
    return m_state == GameState::Computing || m_state == GameState::Stopping || m_state == GameState::ShowingMove || m_state == GameState::AutoFilling
        || m_state == GameState::AnimatingConversion || m_state == GameState::Aborting;
}

#include "moc_game.cpp"
