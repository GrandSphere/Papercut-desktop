#include "TextFinder.h"

#include <QRegularExpression>
#include <algorithm>

namespace papercut {
namespace {

const QRegularExpression &whitespace() {
    static const QRegularExpression re(QStringLiteral("\\s+"));
    return re;
}

QString wordSnippet(const QString &text, int matchStart, int matchEnd) {
    const int last = static_cast<int>(text.size() - 1);
    int start = std::clamp(matchStart, 0, last);
    int end = std::clamp(matchEnd, 0, last);
    while (start > 0 && !text.at(start - 1).isSpace()) --start;
    while (end < text.size() - 1 && !text.at(end + 1).isSpace()) ++end;
    QString out = text.mid(start, end - start + 1);
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return out.trimmed();
}

QString escapeRegex(const QString &s) {
    return QRegularExpression::escape(s);
}

} // namespace

QVector<SearchHit> TextFinder::find(const QVector<QVector<CharBox>> &pages, const QString &query) {
    const QStringList tokens = query.trimmed().split(whitespace(), Qt::SkipEmptyParts);
    if (tokens.isEmpty() || pages.isEmpty()) return {};

    QString patternSrc;
    for (int i = 0; i < tokens.size(); ++i) {
        if (i > 0) patternSrc += QStringLiteral("[^\\s]*\\s?[^\\s]*");
        patternSrc += escapeRegex(tokens[i]);
    }
    QRegularExpression pattern(patternSrc, QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    if (!pattern.isValid()) return {};

    int total = pages.size() - 1;
    for (const auto &chars : pages) total += chars.size();
    if (total <= 0) return {};

    QVector<int> pageOf(total);
    QVector<int> indexOf(total);
    QString text;
    text.reserve(total);
    int pos = 0;
    for (int page = 0; page < pages.size(); ++page) {
        if (page > 0) {
            text.append(QLatin1Char('\n'));
            pageOf[pos] = -1;
            ++pos;
        }
        const auto &chars = pages[page];
        for (int i = 0; i < chars.size(); ++i) {
            text.append(chars[i].c);
            pageOf[pos] = page;
            indexOf[pos] = i;
            ++pos;
        }
    }

    QVector<SearchHit> hits;
    auto it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int first = match.capturedStart();
        const int last = match.capturedEnd() - 1;
        if (first < 0 || last < first) continue;
        int start = first;
        while (start <= last && pageOf[start] < 0) ++start;
        if (start > last) continue;
        int end = last;
        while (end > start && pageOf[end] < 0) --end;
        hits.push_back(SearchHit{pageOf[start], indexOf[start], pageOf[end], indexOf[end],
                                 wordSnippet(text, start, end)});
    }
    return hits;
}

} // namespace papercut
