#include "MarkdownView.h"
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>

MarkdownView::MarkdownView(QQuickItem *parent):QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this,&QQuickItem::widthChanged,this,&MarkdownView::render);
    connect(this,&MarkdownView::styleChanged,this,&MarkdownView::render);
}
void MarkdownView::setMarkdown(const QString &text) {
    if(source==text) return;
    source=text;render();emit contentChanged();
}
void MarkdownView::render() {
    QFont font("Sans Serif");font.setPixelSize(size+1);document.setDefaultFont(font);
    document.setDefaultStyleSheet(QString("body {color:%1;} a {color:%2;} pre {margin:12px;} blockquote {margin-left:18px;} h1 {font-size:24px;} h2 {font-size:19px;} h3 {font-size:16px;} code {font-family:monospace;}").arg(foreground.name(),accent.name()));
    document.setMarkdown(source,QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
    for(auto block=document.begin();block.isValid();block=block.next()) {
        QTextCursor cursor(block);
        auto format=block.blockFormat();
        const int heading=format.headingLevel();
        format.setTopMargin(heading ? (heading==1 ? 0 : 18) : 0);
        format.setBottomMargin(heading ? 8 : 12);
        format.setLineHeight(120,QTextBlockFormat::ProportionalHeight);
        cursor.setBlockFormat(format);
        if(heading) {
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat headingFormat;
            headingFormat.setProperty(QTextFormat::FontPixelSize,heading==1?size+14:heading==2?size+6:size+3);
            cursor.mergeCharFormat(headingFormat);
        }
    }
    document.setTextWidth(qMax(100.0,width()));
    setImplicitHeight(document.size().height()+12);
    update();
}
void MarkdownView::paint(QPainter *painter) {
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text,foreground);
    document.documentLayout()->draw(painter,context);
}
MarkdownHighlighter::MarkdownHighlighter(QObject *parent):QSyntaxHighlighter(parent) {
    connect(this,&MarkdownHighlighter::colorsChanged,this,&QSyntaxHighlighter::rehighlight);
}
void MarkdownHighlighter::setTarget(QQuickTextDocument *target) {
    if(quickDocument==target) return;
    quickDocument=target;
    setDocument(target?target->textDocument():nullptr);emit targetChanged();
}
void MarkdownHighlighter::highlightBlock(const QString &text) {
    QTextCharFormat heading;heading.setForeground(accent);heading.setFontWeight(QFont::DemiBold);
    QTextCharFormat quiet;quiet.setForeground(muted);
    if(text.startsWith('#'))setFormat(0,text.size(),heading);
    if(text.startsWith('>') || text.startsWith("<!--"))setFormat(0,text.size(),quiet);
    if(text=="---" && (currentBlock().blockNumber()==0 || previousBlockState()==1)) {
        setCurrentBlockState(previousBlockState()==1?0:1);setFormat(0,text.size(),quiet);return;
    }
    if(previousBlockState()==1){setCurrentBlockState(1);setFormat(0,text.size(),quiet);return;}
    setCurrentBlockState(0);
    static const QRegularExpression links(R"((\[[^\]]+\]\([^\)]+\)|`[^`]+`))");
    auto matches=links.globalMatch(text);
    while(matches.hasNext()){auto m=matches.next();setFormat(m.capturedStart(),m.capturedLength(),heading);}
}
