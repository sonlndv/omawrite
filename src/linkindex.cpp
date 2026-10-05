#include "linkindex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "markdownhighlighter.h"

void LinkIndex::rebuild(const QString &vaultRoot, const QStringList &filePaths) {
    m_notePaths = filePaths;
    m_forward.clear();
    m_reverse.clear();

    if (vaultRoot.isEmpty())
        return;

    const QDir vaultDir(vaultRoot);
    for (const QString &relative : filePaths) {
        if (!relative.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
            continue;

        QFile file(vaultDir.filePath(relative));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QString text = QString::fromUtf8(file.readAll());

        const QList<MarkdownHighlighter::WikiLink> wikiLinks =
            MarkdownHighlighter::wikiLinks(text);
        QList<Link> links;
        links.reserve(wikiLinks.size());
        for (const MarkdownHighlighter::WikiLink &wikiLink : wikiLinks) {
            const QString resolved = resolveTarget(wikiLink.target);
            links.append({wikiLink.target, resolved});
            if (!resolved.isEmpty())
                m_reverse[resolved].insert(relative);
        }
        m_forward.insert(relative, links);
    }
}

QList<LinkIndex::Link> LinkIndex::forwardLinks(const QString &relativePath) const {
    return m_forward.value(relativePath);
}

QStringList LinkIndex::backlinks(const QString &relativePath) const {
    const QSet<QString> sources = m_reverse.value(relativePath);
    QStringList result(sources.constBegin(), sources.constEnd());
    std::sort(result.begin(), result.end());
    return result;
}

QVariantList LinkIndex::graphModel() const {
    QVariantList edges;
    for (auto it = m_forward.constBegin(); it != m_forward.constEnd(); ++it) {
        for (const Link &link : it.value()) {
            const bool broken = link.resolvedPath.isEmpty();
            edges.append(QVariantMap{
                {QStringLiteral("from"), it.key()},
                {QStringLiteral("to"), broken ? link.rawTarget : link.resolvedPath},
                {QStringLiteral("rawTarget"), link.rawTarget},
                {QStringLiteral("broken"), broken},
            });
        }
    }
    return edges;
}

// Mirrors Backend::resolveWikiLinkTarget's resolution rule: match by filename
// stem (name without extension), case-insensitive, vault-wide; ambiguity
// picks the shallowest path and breaks remaining ties alphabetically.
QString LinkIndex::resolveTarget(const QString &target) const {
    QString bestPath;
    int bestDepth = -1;
    for (const QString &relative : m_notePaths) {
        const QString stem = QFileInfo(relative).completeBaseName();
        if (stem.compare(target, Qt::CaseInsensitive) != 0)
            continue;

        const int depth = relative.count(QLatin1Char('/'));
        if (bestDepth < 0 || depth < bestDepth
                || (depth == bestDepth && relative.compare(bestPath, Qt::CaseInsensitive) < 0)) {
            bestPath = relative;
            bestDepth = depth;
        }
    }
    return bestPath;
}
