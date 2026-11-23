#include "AppTheme.h"
#include "AppWindowFrame.h"
#include "AppUiLayout.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTextBrowser>
#include <QComboBox>
#include <QStyledItemDelegate>
#include <QAbstractItemView>

namespace {
class ComboItemDelegate : public QStyledItemDelegate {
public:
    explicit ComboItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), option.fontMetrics.height() + 12));
        return size;
    }
};
const QStringList ids{"blueprint", "warm", "tricolor"};
QJsonObject colors(int index) {
    static QMap<int, QJsonObject> cache;
    if (cache.contains(index))
        return cache.value(index);
    QFile file(":/themes/" + ids.at(index) + ".json");
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();
    const auto theme = QJsonDocument::fromJson(file.readAll()).object();
    cache.insert(index, theme);
    return theme;
}
QColor color(const QJsonObject &theme, const char *key) {
    return QColor(theme.value(key).toString());
}
} // namespace

QStringList AppTheme::names() {
    return {QStringLiteral("蓝白工程 · Blueprint"), QStringLiteral("暖灰低眩光 · Warm"), QStringLiteral("炭灰三色 · Tricolor")};
}

int AppTheme::current() {
    const auto selected = qApp->property("pinHzTheme");
    return selected.isValid() ? qBound(0, selected.toInt(), ids.size() - 1) : defaultIndex();
}

void AppTheme::prepareComboBox(QComboBox *combo) {
    if (!combo || combo->property("pinHzComboReady").toBool())
        return;
    // Table-cell combos otherwise inherit a compact menu delegate that clips option rows.
    combo->setItemDelegate(new ComboItemDelegate(combo));
    combo->view()->setTextElideMode(Qt::ElideNone);
    combo->view()->ensurePolished();
    combo->setProperty("pinHzComboReady", true);
}

QString AppTheme::adaptHtml(const QString &html) {
    const QJsonObject theme = colors(current());
    if (theme.isEmpty())
        return html;
    QMap<QString, QString> replacements;
    const QMap<QString, QString> legacy{
        {"#000000", "text"},    {"#ffffff", "text"},    {"#0000ff", "accent"},
        {"#ff00ff", "warning"}, {"#ff0000", "error"},   {"#00ff00", "success"},
        {"#00ffff", "teal"},    {"#ffff00", "warning"}, {"#ff8c00", "warning"},
        {"#00b3b3", "teal"},    {"#8a2be2", "purple"}};
    for (auto it = legacy.constBegin(); it != legacy.constEnd(); ++it)
        replacements[it.key()] = theme.value(it.value()).toString();
    for (int i = 0; i < ids.size(); ++i) {
        const auto old = colors(i);
        for (const QString &role :
             {QString("text"), QString("accent"), QString("warning"), QString("error"),
              QString("success"), QString("teal"), QString("purple")})
            replacements[old.value(role).toString().toLower()] = theme.value(role).toString();
    }
    static const QRegularExpression expression("(?<![-\\w])(color\\s*[:=]\\s*[\"']?)(#[0-9a-f]{6})",
                                               QRegularExpression::CaseInsensitiveOption);
    QString result;
    int position = 0;
    auto matches = expression.globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += html.mid(position, match.capturedStart() - position) + match.captured(1);
        result += replacements.value(match.captured(2).toLower(), match.captured(2));
        position = match.capturedEnd();
    }
    return result + html.mid(position);
}

bool AppTheme::apply(int index) {
    if (index < 0 || index >= ids.size())
        return false;
    AppUiLayout::install();
    auto theme = colors(index);
    QFile file(":/themes/common.qss");
    if (theme.isEmpty() || !file.open(QIODevice::ReadOnly))
        return false;
    if (!theme.contains("focus"))
        theme.insert("focus", theme.value("accent"));
    if (!theme.contains("badge"))
        theme.insert("badge", theme.value("selection"));
    if (!theme.contains("badgeText"))
        theme.insert("badgeText", theme.value("accent"));
    if (!theme.contains("secondarySurface"))
        theme.insert("secondarySurface", theme.value("surface"));
    if (!theme.contains("secondaryText"))
        theme.insert("secondaryText", theme.value("text"));
    if (!theme.contains("secondaryBorder"))
        theme.insert("secondaryBorder", theme.value("border"));
    QString stylesheet = QString::fromUtf8(file.readAll());
    for (auto it = theme.constBegin(); it != theme.constEnd(); ++it)
        stylesheet.replace("@" + it.key() + "@", it.value().toString());

    if (!qApp->property("pinHzThemeInitialized").toBool()) {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setProperty("pinHzThemeInitialized", true);
    }
    QPalette palette;
    palette.setColor(QPalette::Window, color(theme, "background"));
    palette.setColor(QPalette::WindowText, color(theme, "text"));
    palette.setColor(QPalette::Base, color(theme, "surface"));
    palette.setColor(QPalette::AlternateBase, color(theme, "alternate"));
    palette.setColor(QPalette::Text, color(theme, "text"));
    palette.setColor(QPalette::Button, color(theme, "surface"));
    palette.setColor(QPalette::ButtonText, color(theme, "text"));
    palette.setColor(QPalette::Mid, color(theme, "border"));
    palette.setColor(QPalette::Highlight, color(theme, "selection"));
    palette.setColor(QPalette::HighlightedText, color(theme, "text"));
    palette.setColor(QPalette::ToolTipBase, color(theme, "surface"));
    palette.setColor(QPalette::ToolTipText, color(theme, "text"));
    palette.setColor(QPalette::Link, color(theme, "accent"));
    for (auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, color(theme, "muted"));
    qApp->setProperty("pinHzTheme", index);
    qApp->setPalette(palette);
    qApp->setStyleSheet(stylesheet);
    AppWindowFrame::refresh();
    // Existing rich-text logs also need new semantic colors when switching themes.
    for (QWidget *widget : qApp->allWidgets()) {
        auto *browser = qobject_cast<QTextBrowser *>(widget);
        if (!browser || browser->document()->isEmpty())
            continue;
        const int scroll = browser->verticalScrollBar()->value();
        const int cursor = browser->textCursor().position();
        browser->setHtml(adaptHtml(browser->toHtml()));
        QTextCursor restored = browser->textCursor();
        restored.setPosition(qMin(cursor, browser->document()->characterCount() - 1));
        browser->setTextCursor(restored);
        browser->verticalScrollBar()->setValue(scroll);
    }
    return true;
}
