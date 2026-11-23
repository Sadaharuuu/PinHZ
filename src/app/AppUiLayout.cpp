#include "AppUiLayout.h"
#include "AppTheme.h"
#include <QApplication>
#include <QComboBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTimer>
#include <QEvent>

namespace {
void fitTable(QTableWidget *table) {
    // QSS header padding is not included reliably in Qt 5.9's automatic width.
    QFont font = table->font();
    font.setBold(true);
    const QFontMetrics metrics(font);
    int width = 0;
    for (int row = 0; row < table->rowCount(); ++row) {
        const auto *item = table->verticalHeaderItem(row);
        width = qMax(width, metrics.width(item ? item->text() : QString::number(row + 1)) + 24);
    }
    auto *vertical = table->verticalHeader();
    vertical->setMinimumWidth(qMax(76, width));
    vertical->setTextElideMode(Qt::ElideNone);
    vertical->setMinimumSectionSize(metrics.height() + 16);
    vertical->setDefaultSectionSize(qMax(36, metrics.height() + 16));
    table->horizontalHeader()->setMinimumHeight(metrics.height() + 16);
    table->horizontalHeader()->setTextElideMode(Qt::ElideNone);
}
class LayoutFilter : public QObject {
public:
    explicit LayoutFilter(QObject *parent) : QObject(parent) {}
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() != QEvent::Show && event->type() != QEvent::StyleChange &&
            event->type() != QEvent::FontChange)
            return false;
        if (auto *combo = qobject_cast<QComboBox *>(object)) {
            // Context-bound timers are cancelled automatically when a widget is destroyed.
            QTimer::singleShot(0, combo, [combo] { AppTheme::prepareComboBox(combo); });
        }
        if (auto *table = qobject_cast<QTableWidget *>(object)) {
            if (!table->property("pinHzHeaderFit").toBool()) {
                table->setProperty("pinHzHeaderFit", true);
                QObject::connect(table->model(), &QAbstractItemModel::headerDataChanged, table,
                                 [table] { fitTable(table); });
                QObject::connect(table->model(), &QAbstractItemModel::rowsInserted, table,
                                 [table] { fitTable(table); });
            }
            QTimer::singleShot(0, table, [table] { fitTable(table); });
        }
        return false;
    }
};
} // namespace
void AppUiLayout::install() {
    if (qApp->property("pinHzLayoutInstalled").toBool())
        return;
    qApp->setProperty("pinHzLayoutInstalled", true);
    qApp->installEventFilter(new LayoutFilter(qApp));
}
