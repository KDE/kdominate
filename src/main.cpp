/**
 * Based on the code of KJumpingCube
 * SPDX-FileCopyrightText: 1998-2000 Matthias Kiefer <matthias.kiefer@gmx.de>
 *
 * SPDX-FileCopyrightText: 2026 Albert Vaca <albertvaka@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kdominate.h"
#include "kdominate_version.h"

#include <KAboutData>
#include <KCrash>
#include <KDBusService>
#include <KLocalizedString>
#define HAVE_KICONTHEME __has_include(<KIconTheme>)
#if HAVE_KICONTHEME
#include <KIconTheme>
#endif
#define HAVE_STYLE_MANAGER __has_include(<KStyleManager>)
#if HAVE_STYLE_MANAGER
#include <KStyleManager>
#endif

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
#if HAVE_KICONTHEME
    KIconTheme::initTheme();
#endif
    QApplication app(argc, argv);
#if HAVE_STYLE_MANAGER
    KStyleManager::initStyle();
#elif defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QApplication::setStyle(QStringLiteral("breeze"));
#endif
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kdominate"));

    KAboutData aboutData(QStringLiteral("kdominate"),
                         i18n("KDominate"),
                         QStringLiteral(KDOMINATE_VERSION_STRING),
                         i18n("Tactical one or two player game"),
                         KAboutLicense::GPL,
                         i18n("(c) 2026, Albert Vaca Cintora"),
                         QString(),
                         QStringLiteral("https://apps.kde.org/kdominate"));
    aboutData.setOrganizationDomain(QByteArray("kde.org"));
    aboutData.addAuthor(i18n("Albert Vaca Cintora"), QString(), QStringLiteral("albertvaka@kde.org"));
    aboutData.addAuthor(i18n("Matthias Kiefer"), i18n("Original KJumpingCube code"), QStringLiteral("matthias.kiefer@gmx.de"));
    aboutData.addAuthor(i18n("Ian Wadham"), i18n("Original KJumpingCube code"), QStringLiteral("iandw.au@gmail.com"));
    aboutData.addAuthor(i18n("Eugene Trounev"), i18n("Graphics"), QStringLiteral("irs_me@hotmail.com"));
    aboutData.addCredit(i18n("Ken Foster"), i18n("Original game idea"), QString());
    aboutData.addCredit(i18n("Sam C. Misemer"), i18n("Original game idea"), QString());

    KAboutData::setApplicationData(aboutData);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kdominate")));

    KCrash::initialize();

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    KDBusService service;

    // All session management is handled in the RESTORE macro
    KDominate *kdominate = new KDominate;
    kdominate->show();
    return app.exec();
}
