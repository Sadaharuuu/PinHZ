#ifndef APPFILETOOLS_H
#define APPFILETOOLS_H

#include "AppCalcCheck.h"
#include <functional>

namespace AppFileTools {
struct Range {
    qint64 start = 0;
    qint64 endExclusive = 0; // Internal read boundary; the UI uses an inclusive end address.
    qint64 length() const {
        return endExclusive - start;
    }
    QString description() const {
        return length() == 0 ? QStringLiteral("空范围（起始偏移 %1）").arg(start)
                             : QStringLiteral("[%1, %2]").arg(start).arg(endExclusive - 1);
    }
};
enum RangeMode { ToEnd, EndAddress, Length };
using Progress = std::function<bool(qint64 done, qint64 total)>;

bool parseNumber(const QString &text, bool hex, qint64 &number);
bool resolveRange(qint64 fileSize, const QString &start, RangeMode mode, const QString &end,
                  const QString &length, bool hex, Range &range, QString &error);
bool checksumFile(const QString &path, const Range &range, const AppCalcCheck::Settings &settings,
                  QByteArray &result, QString &error, const Progress &progress = Progress());
bool splitFile(const QString &source, const QString &destination, const Range &range,
               QString &error, const Progress &progress = Progress());
} // namespace AppFileTools
#endif
