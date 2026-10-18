#pragma once

#include "ViewerSettings.h"

#include <functional>

namespace papercut {

class Database;

class SettingsRepository {
public:
    SettingsRepository();

    ViewerSettings snapshot;

    ViewerSettings get() { return snapshot; }

    void update(const std::function<ViewerSettings(ViewerSettings)> &transform);

    void migrateFromDatabaseIfEmpty(Database &db);

private:
    void load();
    void save() const;
};

} // namespace papercut
