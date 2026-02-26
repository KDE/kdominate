# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-3-Clause

ecm_setup_version(${KDOMINATE_VERSION}
    VARIABLE_PREFIX KDOMINATE
    VERSION_HEADER kdominate_version.h
)

add_executable(kdominate)

file(GLOB MAP_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/maps/*.map")

qt_add_resources(kdominate "maps"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${MAP_FILES}
)

target_sources(kdominate PRIVATE
    kdominate_board.cpp
    kdominate_board.h
    kdominate_ai.cpp
    kdominate_ai.h
    game.cpp
    game.h
    kboardwidget.cpp
    kboardwidget.h
    ktilewidget.cpp
    ktilewidget.h
    kdominate.cpp
    kdominate.h
    main.cpp
    settingswidget.h

    kdominate.qrc
)

ecm_qt_declare_logging_category(kdominate
    HEADER kdominate_debug.h
    IDENTIFIER KDOMINATE_LOG
    CATEGORY_NAME kdominate
    DESCRIPTION "KDominate game"
    EXPORT KDOMINATE
)

ki18n_wrap_ui(kdominate settings.ui)

kconfig_add_kcfg_files(kdominate prefs.kcfgc)

file(GLOB ICONS_SRCS "${CMAKE_SOURCE_DIR}/icons/*-apps-kdominate.png")
ecm_add_app_icon(kdominate ICONS ${ICONS_SRCS})

if(APPLE)
    set_target_properties(kdominate PROPERTIES
           MACOSX_BUNDLE_DISPLAY_NAME "Kdominate"
        MACOSX_BUNDLE_BUNDLE_NAME "Kdominate"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Kdominate ${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.kdominate"
        MACOSX_BUNDLE_COPYRIGHT "2024 The Kdominate Developers")
endif()


target_link_libraries(kdominate
    KDEGames6
    KF6::CoreAddons
    KF6::Crash
    KF6::DBusAddons
    KF6::I18n
    KF6::XmlGui
    KF6::IconThemes
    Qt6::Svg
)

install(TARGETS kdominate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(PROGRAMS org.kde.kdominate.desktop  DESTINATION  ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kdominate.appdata.xml DESTINATION  ${KDE_INSTALL_METAINFODIR})
install(FILES kdominate.kcfg  DESTINATION  ${KDE_INSTALL_KCFGDIR})

ecm_qt_install_logging_categories(
    EXPORT KDOMINATE
    FILE kdominate.categories
    DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
)
