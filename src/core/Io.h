#pragma once

#include <QString>
#include <functional>
#include <QIODevice>

namespace papercut {

bool copyAtomically(const QString &dest, const QString &tmp, const std::function<QIODevice *()> &open);

} // namespace papercut
