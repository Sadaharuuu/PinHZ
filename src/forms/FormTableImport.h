#ifndef FORMTABLEIMPORT_H
#define FORMTABLEIMPORT_H
#include "CommonDefine.h"
#include <QDialog>
#include <QTextEdit>
#include <QMimeData>

class TablePasteEdit : public QTextEdit {
    Q_OBJECT
public:
    explicit TablePasteEdit(QWidget *parent = nullptr) : QTextEdit(parent) {
        setAcceptRichText(false);
    }
    static bool clipboardText(const QMimeData *mime, QString &text, QString &error);
signals:
    void pasteError(QString error);

protected:
    void insertFromMimeData(const QMimeData *mime) override;
};
namespace Ui {
class FormTableImport;
}
class FormTableImport : public QDialog {
    Q_OBJECT
public:
    explicit FormTableImport(Endian endian, QWidget *parent = nullptr);
    ~FormTableImport();
    QList<s_itemUnit> items() const {
        return m_items;
    }
    static bool parseGrid(QString text, QList<QStringList> &rows, QString &error);

private:
    Ui::FormTableImport *ui;
    Endian m_endian;
    QList<QStringList> m_rows;
    QList<s_itemUnit> m_previewItems;
    QList<s_itemUnit> m_items;
    void parse();
    void preview();
    void validatePreview();
};
#endif
