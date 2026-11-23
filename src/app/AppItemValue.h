#ifndef APPITEMVALUE_H
#define APPITEMVALUE_H

#include "CommonDefine.h"
#include <QJsonObject>

class AppItemValue {
public:
    static int byteSize(DataType type);
    static bool identity(const QString &formula);
    static bool editable(const s_itemUnit &item);
    static bool normalizeHex(QString text, DataType type, Endian endian, QString &hex,
                             QString &error, bool allowNarrow = false);
    static bool validateConfig(const s_itemUnit &item, QString &error);
    static bool display(const s_itemUnit &item, Endian endian, QString &text, QString &error);
    static bool validateInput(const s_itemUnit &item, Endian endian, QString &text, QString &error);
    static bool fromDisplay(const s_itemUnit &item, QString text, Endian endian, QString &hex,
                            QString &error);
    static void writeRules(QJsonObject &json, const s_itemUnit &item);
    static void readRules(const QJsonObject &json, s_itemUnit &item);
};

#endif
