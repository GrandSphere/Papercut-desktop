#pragma once

#include <QString>

namespace papercut {
namespace JumpBookmarks {

inline const QString kJump1 = QStringLiteral("Quick jump 1");
inline const QString kJump2 = QStringLiteral("Quick jump 2");

inline bool isJumpName(const QString &name) { return name == kJump1 || name == kJump2; }

} // namespace JumpBookmarks
} // namespace papercut
