#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>

// In-memory forward/reverse index of [[wikilink]] targets across a vault.
// Deliberately minimal: it exists to feed the graph view (Backend::linkGraph),
// not to be a full backlinks product. No persistence -- rebuild() re-scans
// vault note content from disk; callers decide when that is cheap enough.
class LinkIndex {
public:
    struct Link {
        QString rawTarget;    // target as typed inside [[...]]
        QString resolvedPath; // vault-relative path, empty if the target is broken
    };

    // Rebuilds the whole index from scratch.
    // `vaultRoot` is the absolute vault directory; `filePaths` lists every
    // vault-relative file path (any extension -- these are the possible link
    // targets). Only paths ending in ".md" are read and parsed for outgoing
    // links; non-markdown files can still be resolved *to* as targets.
    void rebuild(const QString &vaultRoot, const QStringList &filePaths);

    // Outgoing links for one note, in source order. Empty if the note has
    // none or is unknown.
    QList<Link> forwardLinks(const QString &relativePath) const;

    // Vault-relative paths of every note with a resolved link to this path.
    QStringList backlinks(const QString &relativePath) const;

    // QML-facing edge list the graph view consumes: one entry per outgoing
    // link, each a map of {from, to, rawTarget, broken}. `to` is the resolved
    // path when the link resolves, otherwise the raw typed target.
    QVariantList graphModel() const;

private:
    QString resolveTarget(const QString &target) const;

    QStringList m_notePaths; // all known vault-relative file paths
    QHash<QString, QList<Link>> m_forward; // note -> outgoing links
    QHash<QString, QSet<QString>> m_reverse; // resolved note -> notes linking to it
};
