#ifndef APPTHEME_H
#define APPTHEME_H

#include <QString>
#include <QStringList>

class QComboBox;

class AppTheme {
public:
    static QStringList names();
    static int defaultIndex() {
        return 2;
    }
    static bool apply(int index);
    static int current();
    static QString adaptHtml(const QString &html);
    static void prepareComboBox(QComboBox *combo);
};

#endif
