#pragma once

#include "DocumentController.h"

namespace papercut {

class FolioController : public DocumentController {
    Q_OBJECT
    Q_PROPERTY(int folioIndex READ folioIndex NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList visibleItems READ visibleItems NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList allItems READ allItems NOTIFY itemsChanged)

public:
    explicit FolioController(QObject *parent = nullptr);

    bool inFolio() const override { return true; }
    ViewerSettings viewingSettings() const override;
    QString shareSourcePath() const override { return currentCopy(); }

    int folioIndex() const { return m_folioIndex; }
    QVariantList visibleItems() const;
    QVariantList allItems() const;

    Q_INVOKABLE void loadStart();
    Q_INVOKABLE void addFromPaths(const QStringList &paths);
    Q_INVOKABLE bool storesLastPosition() const;
    Q_INVOKABLE void showFolioIndex(int index);
    Q_INVOKABLE void saveFolioSnapshot(int page, double scrollX, double scrollY);
    Q_INVOKABLE void setFolioHidden(const QVariantList &ids, bool hidden);
    Q_INVOKABLE void deleteFolioItems(const QVariantList &ids);
    Q_INVOKABLE void reorderFolio(const QVariantList &ids);
    Q_INVOKABLE void setFolioStart(qint64 id);
    Q_INVOKABLE QString currentCopy() const;
    Q_INVOKABLE void refreshItems();
    // Save folio position when store-last on.
    Q_INVOKABLE void rememberPosition();

signals:
    void itemsChanged();
    void itemsAdded(int count);

protected:
    void mutateView(const std::function<ViewerSettings(ViewerSettings)> &transform) override;
    QString exportSource() const override { return currentCopy(); }
    void resumeAfterPassword(const OpenData &data) override;
    bool clearViewWhenNoDocument() const override { return false; }
    void onFolioSwipe(int delta) override;
    void onRotationsChanged() override;

private:
    QVector<FolioItem> visible() const;
    void clearCurrent();
    void openItem(const FolioItem &item);
    void finishOpen(const FolioItem &item, const OpenData &data);
    ViewerSettings folioViewSettings(const FolioItem &item, const ViewerSettings &global) const;
    int startFolioIndex(const QVector<FolioItem> &items, const ViewerSettings &s) const;
    QVariantMap itemToMap(const FolioItem &i) const;

    QVector<FolioItem> m_items;
    std::optional<ViewerSettings> m_live;
    int m_folioIndex = 0;
    qint64 m_currentId = 0;
    int m_openGen = 0;
};

} // namespace papercut
