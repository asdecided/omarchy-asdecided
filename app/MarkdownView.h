#pragma once
#include <QQuickPaintedItem>
#include <QTextDocument>
#include <QSyntaxHighlighter>
#include <QQuickTextDocument>

class LocalDocument : public QTextDocument {
public:
    using QTextDocument::QTextDocument;
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
};

class MarkdownView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString markdown READ markdown WRITE setMarkdown NOTIFY contentChanged)
    Q_PROPERTY(QColor textColor MEMBER foreground NOTIFY styleChanged)
    Q_PROPERTY(QColor linkColor MEMBER accent NOTIFY styleChanged)
    Q_PROPERTY(int fontSize MEMBER size NOTIFY styleChanged)
public:
    explicit MarkdownView(QQuickItem *parent=nullptr);
    QString markdown() const { return source; }
    void setMarkdown(const QString &text);
    void paint(QPainter *painter) override;
signals:
    void contentChanged();
    void styleChanged();
private:
    void render();
    QString source;
    QColor foreground=QColor("#d9dce1"),accent=QColor("#d5b27b");
    int size=15;
    LocalDocument document;
};

class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(QColor accentColor MEMBER accent NOTIFY colorsChanged)
    Q_PROPERTY(QColor mutedColor MEMBER muted NOTIFY colorsChanged)
public:
    explicit MarkdownHighlighter(QObject *parent=nullptr);
    QQuickTextDocument* target() const { return quickDocument; }
    void setTarget(QQuickTextDocument *target);
signals:
    void targetChanged();
    void colorsChanged();
protected:
    void highlightBlock(const QString &text) override;
private:
    QQuickTextDocument *quickDocument=nullptr;
    QColor accent=QColor("#d5b27b"), muted=QColor("#929aa6");
};
