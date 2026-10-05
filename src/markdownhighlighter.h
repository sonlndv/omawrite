#pragma once

#include <QRegularExpression>
#include <QString>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *document);

    void setDarkMode(bool darkMode);
    void setColors(const QString &background, const QString &foreground, const QString &accent);
    void setSearch(const QString &query, int currentMatchStart);
    void refreshFont();

    struct Span {
        int start;
        int length;
    };

    enum class InlineKind { Bold, Italic, Link };

    struct InlineMarkup {
        InlineKind kind;
        Span content;
        Span markers[2];
    };

    // A [[Note Name]] or [[Note Name|alias]] wikilink. `whole` covers the
    // entire bracketed span (for highlighting); `target` is the note name to
    // resolve (the alias, if present, is cosmetic only and not part of it).
    struct WikiLink {
        Span whole;
        QString target;
    };

    // Single source of truth for inline markdown spans: the highlighter uses it
    // to style content and hide markers, and the editor uses it (via
    // Backend::hiddenRangesAt) to skip the caret over the hidden markers.
    static QList<InlineMarkup> inlineMarkup(const QString &text);

    // Single source of truth for wikilink spans: the highlighter uses it to
    // style the token, and Backend uses it (via Backend::wikiLinkTargetAt) to
    // resolve a document position to a link target.
    static QList<WikiLink> wikiLinks(const QString &text);

protected:
    void highlightBlock(const QString &text) override;

private:
    void rebuildFormats();
    void highlightMarkers(const QString &text);
    void highlightInline(const QString &text);
    void highlightWikiLinks(const QString &text);
    void highlightSearch(const QString &text);

    bool m_darkMode = true;
    QString m_customBackground;
    QString m_customForeground;
    QString m_customAccent;
    QTextCharFormat m_markerFormat;
    QTextCharFormat m_hiddenMarkerFormat;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_quoteFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_wikiLinkFormat;
    QString m_searchQuery;
    int m_currentMatchStart = -1;
    QTextCharFormat m_searchFormat;
    QTextCharFormat m_currentSearchFormat;
};
