/**
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "game.h"
#include "ui_settings.h"

class SettingsWidget : public QWidget, public Ui::Settings
{
public:
    explicit SettingsWidget(QWidget *parent)
        : QWidget(parent)
    {
        setupUi(this);
        // Populate map combo from the :/maps/ resource directory
        QStringList files = Game::availableMapFiles();
        for (const QString &file : files) {
            QString path = QStringLiteral(":/maps/") + file;
            QString name = Game::mapDisplayName(path);
            kcfg_MapIndex->addItem(name);
        }
        // Themes are hardcoded and should match the same order as the enum in kboardwidget.h
        kcfg_ThemeIndex->addItem(i18n("Simple"));
        kcfg_ThemeIndex->addItem(i18n("Plastic"));
        kcfg_ThemeIndex->addItem(i18n("Glass"));
    }
};
