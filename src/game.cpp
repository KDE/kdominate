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
#include "prefs.h"

#include <KConfigDialog>
#include <KIO/FileCopyJob>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryFile>
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

Game::Game(KBoardWidget *view, QWidget *parent)
    : QObject((QObject *)parent)
    , m_state(GameState::Idle)
    , m_moveNo(0)
    , m_interrupting(false)
    , m_parent(parent)
    , m_view(view)
    , m_settingsPage(nullptr)
    , m_selected(false)
    , computerPlOne(false)
    , computerPlTwo(false)
    , m_pauseForComputer(false)
    , m_undoIndex(0)
{
    m_board = new KDominateBoard();
    m_ai = new KDominateAi(this);

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
    if (isBusy() && (action != BUTTON) && (action != Action::NEW)) {
        m_view->showPopup(i18n("Sorry, doing a move..."));
        return;
    }

    switch (action) {
    case Action::NEW:
        newGame();
        break;
    case Action::HINT:
        if (!m_board->isGameOver() && !isComputer(m_board->currentPlayer())) {
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

QString Game::winnerString() const
{
    int w = m_board->winner();
    if (w == 0) {
        return i18n("Draw!");
    } else {
        return i18n("Player %1 wins!", w);
    }
}

void Game::showWinner()
{
    Q_EMIT buttonChange(false, false, i18n("Game over"));
    KTileWidget::enableClicks(false);
    KMessageBox::information(m_view, winnerString(), i18n("Game Over"));
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
    qCDebug(KDOMINATE_LOG) << "NEW SETTINGS state:" << int(m_state) << "mapIndex:" << Prefs::mapIndex();
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
    bool reColorTiles = m_view->loadSettings();
    if (reColorTiles) {
        Q_EMIT statusUpdated();
    }
}

void Game::loadPlayerSettings()
{
    bool oldComputerPlayer = isComputer(m_board->currentPlayer());

    m_pauseForComputer = Prefs::pauseForComputer();
    computerPlOne = Prefs::computerPlayer1();
    computerPlTwo = Prefs::computerPlayer2();

    if (isComputer(m_board->currentPlayer()) && (!oldComputerPlayer)) {
        qCDebug(KDOMINATE_LOG) << "New computer player set: must wait.";
        m_state = GameState::WaitingForButton;
    }
}

void Game::startHumanMove(int x, int y)
{
    bool humanPlayer = (!isComputer(m_board->currentPlayer()));
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
        if (cellOwner == m_board->currentPlayer()) {
            m_selected = true;
            m_selectedOrigin = QPoint(x, y);
            m_view->selectTile(m_selectedOrigin);
            highlightValidDestinations(m_selectedOrigin);
            Q_EMIT statusMessage(i18n("Select destination"), false);
        }
    } else {
        // Second click: select destination or change selection
        if (cellOwner == m_board->currentPlayer()) {
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

    qCDebug(KDOMINATE_LOG) << "setUpNextTurn" << m_board->currentPlayer() << computerPlOne << computerPlTwo << "pause:" << m_pauseForComputer
                           << "state:" << int(m_state);

    if (isComputer(m_board->currentPlayer())) {
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
            Q_EMIT buttonChange(false, false, i18n("Player %1", m_board->currentPlayer()));
        }
    }
}

void Game::computeMove()
{
    m_view->setWaitCursor();
    m_state = GameState::Computing;
    setStopAction();
    Q_EMIT setAction(Action::HINT, false);
    int skill = (m_board->currentPlayer() == 1) ? Prefs::skill1() : Prefs::skill2();
    m_ai->setDepth(skill + 2); // skill 0..4 maps to depth 2..6
    if (isComputer(m_board->currentPlayer())) {
        Q_EMIT statusMessage(i18n("Computer player %1 is moving", m_board->currentPlayer()), false);
    }
    m_ai->getMove(*m_board, m_board->currentPlayer());
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
    m_view->startComputerNextMoveAnimation(origin, dest);
    m_state = GameState::ShowingMove;
    setStopAction();
}

void Game::showingDone(QPoint origin, QPoint dest)
{
    if (isComputer(m_board->currentPlayer())) {
        Q_EMIT statusMessage(QString(), false);
        m_moveNo++;
        doMove(origin, dest);
    } else {
        // Finish Hint animation
        moveDone();
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
    bool validMovement = m_board->move(origin, dest);
    if (!validMovement) {
        qCWarning(KDOMINATE_LOG) << "INVALID MOVE attempted:" << origin << "->" << dest;
        moveDone();
        setUpNextTurn();
        return;
    }

    // Do not update the tiles' view, the animation takes care of it
    Owner owner = boardCellToOwner(m_board->at(dest));
    int diffx = qAbs(origin.x() - dest.x());
    int diffy = qAbs(origin.y() - dest.y());
    bool jumpMovement = (diffx == 2 || diffy == 2);
    if (jumpMovement) {
        m_view->startJumpAnimation(dest, origin, m_board->lastConvertedTiles(), m_board->lastAutofilledTiles(), owner);
    } else {
        m_view->startCloneAnimation(dest, m_board->lastConvertedTiles(), m_board->lastAutofilledTiles(), owner);
    }

    m_state = GameState::AnimatingMove;
}

void Game::finishMove()
{
    updateAllTiles();

    Q_EMIT statusUpdated();

    moveDone();

    if (m_board->isGameOver()) {
        showWinner();
    } else {
        setUpNextTurn();
    }
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
    qCDebug(KDOMINATE_LOG) << "BUTTON CLICK m_state:" << int(m_state);
    if (m_board->isGameOver()) {
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
        if (computerPlOne && computerPlTwo) {
            m_interrupting = true;
        }
        m_view->killAnimation();
        showingDone(m_pendingMoveOrigin, m_pendingMoveDest);
    } else if (m_state == GameState::AnimatingMove) {
        m_view->killAnimation();
        finishMove();
    }
}

void Game::setStopAction()
{
    if ((!m_pauseForComputer) && (!m_interrupting) && (computerPlOne && computerPlTwo)) {
        Q_EMIT buttonChange(true, true, i18n("Interrupt game"));
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
    if (newGameOK()) {
        shutdown();
        m_view->setNormalCursor();
        m_view->hidePopup();
        loadImmediateSettings();
        loadPlayerSettings();
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
    if ((m_moveNo == 0) || m_board->isGameOver()) {
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
        m_view->setSize(m_board->size());
    }

    m_selected = false;

    m_state = GameState::Idle;

    // Clear undo history
    m_undoList.clear();
    m_undoIndex = 0;

    // Update the display to show initial board state
    updateAllTiles();

    Q_EMIT statusUpdated();
}

bool Game::undo()
{
    if (m_undoIndex <= 0) {
        return false;
    }
    m_undoIndex--;
    MoveRecord &snap = m_undoList[m_undoIndex];

    m_board->undo();
    m_moveNo--;

    updateAllTiles();

    Q_EMIT statusUpdated();

    // Highlight the move that was undone
    m_view->timedTileHighlight(snap.origin);

    m_interrupting = isComputer(m_board->currentPlayer());
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

    updateAllTiles();

    Q_EMIT statusUpdated();

    m_view->timedTileHighlight(snap.dest);

    if (m_board->isGameOver()) {
        showWinner();
        return false;
    }

    m_interrupting = isComputer(m_board->currentPlayer());
    m_state = GameState::Idle;
    setUpNextTurn();
    return (m_undoIndex < m_undoList.size());
}

void Game::shutdown()
{
    m_view->killAnimation();
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

void Game::animationDone(int index)
{
    Q_UNUSED(index);
    if (m_state == GameState::ShowingMove) {
        showingDone(m_pendingMoveOrigin, m_pendingMoveDest);
    } else if (m_state == GameState::AnimatingMove) {
        finishMove();
    }
}

void Game::updateTile(QPoint p)
{
    int cell = m_board->at(p);
    Owner owner = boardCellToOwner(cell);
    m_view->displayTile(p, owner);
}

void Game::updateAllTiles()
{
    int size = m_board->size();
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
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

    QList<QPoint> cloneTiles;
    QList<QPoint> jumpTiles;
    for (const QPoint &d : cloneDirs) {
        QPoint dest = origin + d;
        if (m_board->inBounds(dest) && m_board->at(dest) == 0) {
            cloneTiles.append(dest);
        }
    }
    for (const QPoint &d : jumpDirs) {
        QPoint dest = origin + d;
        // Exclude jumps over walls
        if (m_board->inBounds(dest) && m_board->at(dest) == 0 && m_board->at(origin + d / 2) != -1) {
            jumpTiles.append(dest);
        }
    }
    m_view->highlightValidMoves(m_board->currentPlayer(), cloneTiles, jumpTiles);
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
    return m_state == GameState::Computing || m_state == GameState::Stopping || m_state == GameState::ShowingMove || m_state == GameState::AnimatingMove
        || m_state == GameState::Aborting;
}

#include "moc_game.cpp"
