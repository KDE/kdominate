/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QLabel>

#include <KXmlGuiWindow>
#include <game.h>

class QAction;
class KBoardWidget;
class QPushButton;

class KDominate : public KXmlGuiWindow
{
    Q_OBJECT

public:
    KDominate();

public Q_SLOTS:
    void setAction(const Action action, const bool onOff);

private:
    Game *m_game;
    KBoardWidget *m_view = nullptr;
    QLabel *currentPlayer = nullptr;
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *stopAction = nullptr;
    QAction *hintAction = nullptr;
    QPushButton *actionButton = nullptr;

private Q_SLOTS:
    void changePlayerColor(int newPlayer);
    void changeButton(bool enabled, bool stop = false, const QString &caption = QString());
    void statusMessage(const QString &message, bool timed);
};
