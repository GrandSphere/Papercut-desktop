#pragma once

#include <QDir>
#include <QString>

namespace papercut {

struct AppPaths {
    static constexpr const char kDatabase[] = "Papercut.db";
    static constexpr const char kKeybinds[] = "keybinds.json";
    static constexpr const char kSettings[] = "settings.json";

    static QString dataRoot();
    static QString configRoot();
    static QString filesRoot();
    static void setFilesRootOverride(const QString &path);

    static QDir temp();
    static QDir folio();
    static QString database();
    static QString keybinds();
    static QString settingsFile();
    static QString exportPdf();
    static QString jsonShare(const QString &name);

    static void migrateLegacyLayout();

    static QString root() { return dataRoot(); }
};

} // namespace papercut
