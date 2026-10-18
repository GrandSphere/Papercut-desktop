#include "SettingsRepository.h"

#include "BackupJson.h"
#include "Database.h"
#include "core/AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace papercut {

SettingsRepository::SettingsRepository() { load(); }

void SettingsRepository::load() {
    snapshot = ViewerSettings{};
    QFile f(AppPaths::settingsFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    snapshot = BackupJson::mergeSettings(snapshot, QString::fromUtf8(f.readAll()));
}

void SettingsRepository::save() const {
    QDir().mkpath(AppPaths::configRoot());
    QSaveFile f(AppPaths::settingsFile());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("papercut: could not write settings.json");
        return;
    }
    f.write(BackupJson::settingsToJson(snapshot).toUtf8());
    f.commit();
}

void SettingsRepository::update(const std::function<ViewerSettings(ViewerSettings)> &transform) {
    snapshot = transform(snapshot);
    AppPaths::setFilesRootOverride(snapshot.filesRoot);
    save();
}

void SettingsRepository::migrateFromDatabaseIfEmpty(Database &db) {
    if (QFileInfo::exists(AppPaths::settingsFile())) return;
    snapshot = BackupJson::mergeSettings(snapshot, BackupJson::settingsToJson(loadSettings(db.db())));
    const ViewerSettings legacy = loadSettings(db.db());
    snapshot.lastPdfUri = legacy.lastPdfUri;
    snapshot.lockZoom = legacy.lockZoom;
    snapshot.lockSideScroll = legacy.lockSideScroll;
    snapshot.originalColors = legacy.originalColors;
    snapshot.zoomMode = legacy.zoomMode;
    snapshot.customZoomPercent = legacy.customZoomPercent;
    save();
}

} // namespace papercut
