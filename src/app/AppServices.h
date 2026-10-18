#pragma once

#include "data/BookmarkRepository.h"
#include "data/Database.h"
#include "data/FolioRepository.h"
#include "data/FolioStore.h"
#include "data/SettingsRepository.h"

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <memory>

namespace papercut {

class AppServices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settingsMap NOTIFY settingsChanged)
    Q_PROPERTY(QColor bgColor READ bgColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor pageFgColor READ pageFgColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor appFontColor READ appFontColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor actionColor READ actionColor NOTIFY settingsChanged)
    Q_PROPERTY(QColor highlightColor READ highlightColor NOTIFY settingsChanged)
    Q_PROPERTY(double fontScale READ fontScale NOTIFY settingsChanged)
    Q_PROPERTY(QString uiFontFamily READ uiFontFamily NOTIFY settingsChanged)
    Q_PROPERTY(int uiFontWeight READ uiFontWeight NOTIFY settingsChanged)
    Q_PROPERTY(QString iconFontFamily READ iconFontFamily CONSTANT)
    Q_PROPERTY(QStringList systemFontFamilies READ systemFontFamilies CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(int versionCode READ versionCode CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir NOTIFY settingsChanged)
    Q_PROPERTY(QString tempDir READ tempDir NOTIFY settingsChanged)
    Q_PROPERTY(double uiScale READ fontScale NOTIFY settingsChanged)

public:
    static AppServices *instance();
    static AppServices *create();
    ~AppServices() override;

    Database &database() { return m_db; }
    SettingsRepository &settingsRepo() { return *m_settings; }
    BookmarkRepository &bookmarks() { return *m_bookmarks; }
    FolioRepository &folio() { return *m_folio; }
    FolioStore &folioStore() { return *m_folioStore; }

    ViewerSettings settings() const { return m_settings->snapshot; }
    void updateSettings(const std::function<ViewerSettings(ViewerSettings)> &transform);
    void reloadSettings();

    QVariantMap settingsMap() const;
    QColor bgColor() const;
    QColor pageFgColor() const;
    QColor appFontColor() const;
    QColor actionColor() const;
    QColor highlightColor() const;
    double fontScale() const;
    QString uiFontFamily() const;
    int uiFontWeight() const;
    QString documentUserCss() const;
    QString iconFontFamily() const;
    QStringList systemFontFamilies() const;
    static void registerUiFont(const QString &id, const QString &qtFamily);
    static void setIconFontFamily(const QString &family);
    QString version() const;
    int versionCode() const;
    QString dataDir() const;
    QString tempDir() const;

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant setting(const QString &key) const;
    Q_INVOKABLE int fontPx(int base) const;
    Q_INVOKABLE void saveSettingsDraft(const QVariantMap &draft);
    Q_INVOKABLE QString defaultFilesRoot() const;
    Q_INVOKABLE QVariantList documents() const;
    Q_INVOKABLE void deleteDocuments(const QVariantList &uris);
    Q_INVOKABLE void clearAllBookmarks();
    Q_INVOKABLE QString exportBackup(const QString &kind) const;
    Q_INVOKABLE QString importBackup(const QString &path);
    Q_INVOKABLE QVariantList storedFiles() const;
    Q_INVOKABLE void deleteStored(const QVariantList &entries);

    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE bool openUrl(const QString &url) const;
    Q_INVOKABLE QString localPath(const QUrl &url) const;
    Q_INVOKABLE QUrl fileUrl(const QString &path) const;
    Q_INVOKABLE QString fileName(const QString &path) const;
    Q_INVOKABLE bool copyFileTo(const QString &src, const QString &destPath) const;
    Q_INVOKABLE QColor argb(int value) const;
    Q_INVOKABLE int toArgb(const QColor &c) const;
    Q_INVOKABLE QString readTextFile(const QString &path) const;
    Q_INVOKABLE bool writeTextFile(const QString &path, const QString &text) const;
    Q_INVOKABLE QStringList openFilters() const;
    Q_INVOKABLE QUrl suggestedFile(const QString &name) const;
    Q_INVOKABLE bool revealFile(const QString &path) const;
    Q_INVOKABLE QString exportFileName(const QString &kind) const;

signals:
    void settingsChanged();
    void aboutToDeleteFiles(const QStringList &paths);
    void fileRequested(const QString &file, int page, const QString &password);

private:
    explicit AppServices(QObject *parent = nullptr);
    Database m_db;
    std::unique_ptr<SettingsRepository> m_settings;
    std::unique_ptr<BookmarkRepository> m_bookmarks;
    std::unique_ptr<FolioStore> m_folioStore;
    std::unique_ptr<FolioRepository> m_folio;
};

} // namespace papercut
