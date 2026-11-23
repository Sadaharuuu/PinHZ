#include "AppFileTools.h"
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
bool prepare(QFile &file, const AppFileTools::Range &range, QString &error) {
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法打开源文件：%1").arg(file.errorString());
        return false;
    }
    if (range.start < 0 || range.endExclusive < range.start || range.endExclusive > file.size()) {
        error = QStringLiteral("范围越界或文件大小已改变，请重新设置范围。");
        return false;
    }
    if (!file.seek(range.start)) {
        error = QStringLiteral("文件定位失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

bool report(const AppFileTools::Progress &progress, qint64 done, qint64 total, QString &error) {
    if (progress && !progress(done, total)) {
        error = QStringLiteral("操作已取消。");
        return false;
    }
    return true;
}

bool readBlock(QFile &file, qint64 remaining, QByteArray &data, QString &error) {
    const qint64 requested = qMin(qint64(1024 * 1024), remaining);
    data = file.read(requested);
    if (data.size() != requested) {
        error = QStringLiteral("源文件读取不完整或发生错误，文件可能已改变：%1")
                    .arg(file.errorString());
        return false;
    }
    return true;
}
} // namespace

bool AppFileTools::parseNumber(const QString &text, bool hex, qint64 &number) {
    QString value = text.trimmed();
    const bool prefix = value.startsWith("0x", Qt::CaseInsensitive);
    if (prefix)
        value.remove(0, 2);
    static const QRegularExpression digits("^[0-9A-Fa-f]+$");
    if (!digits.match(value).hasMatch())
        return false;
    bool ok = false;
    number = value.toLongLong(&ok, prefix || hex ? 16 : 10);
    return ok && number >= 0;
}

bool AppFileTools::resolveRange(qint64 fileSize, const QString &start, RangeMode mode,
                                const QString &end, const QString &length, bool hex, Range &range,
                                QString &error) {
    error.clear();
    Range resolved;
    if (fileSize < 0) {
        error = QStringLiteral("文件大小无效。");
        return false;
    }
    if (!start.isEmpty() && !parseNumber(start, hex, resolved.start)) {
        error = QStringLiteral("起始地址无效，请输入非负整数。");
        return false;
    }
    if (resolved.start > fileSize) {
        error = QStringLiteral("起始地址超出文件大小。");
        return false;
    }
    resolved.endExclusive = fileSize;
    if (mode == EndAddress) {
        qint64 endAddress = 0;
        if (!parseNumber(end, hex, endAddress)) {
            error = QStringLiteral("结束地址无效，请输入非负整数。");
            return false;
        }
        if (endAddress < resolved.start || endAddress >= fileSize) {
            error =
                QStringLiteral("结束地址必须大于等于起始地址，且小于文件大小（包含结束地址）。");
            return false;
        }
        // Validate against fileSize before adding one, including for 64-bit offsets.
        resolved.endExclusive = endAddress + 1;
    } else if (mode == Length) {
        qint64 count = 0;
        if (!parseNumber(length, hex, count)) {
            error = QStringLiteral("长度无效，请输入非负整数。");
            return false;
        }
        // Subtraction avoids overflow when adding a user-supplied 64-bit length.
        if (count > fileSize - resolved.start) {
            error = QStringLiteral("起始地址加长度超出文件大小。");
            return false;
        }
        resolved.endExclusive = resolved.start + count;
    } else if (mode != ToEnd) {
        error = QStringLiteral("范围模式无效。");
        return false;
    }
    if (resolved.endExclusive < resolved.start || resolved.endExclusive > fileSize) {
        error = QStringLiteral("处理范围超出文件大小。");
        return false;
    }
    range = resolved;
    return true;
}

bool AppFileTools::checksumFile(const QString &path, const Range &range,
                                const AppCalcCheck::Settings &settings, QByteArray &result,
                                QString &error, const Progress &progress) {
    result.clear();
    error.clear();
    AppCalcCheck calculator(settings);
    if (!calculator.isValid()) {
        error = QStringLiteral("校验参数无效。");
        return false;
    }
    QFile file(path);
    if (!prepare(file, range, error))
        return false;
    qint64 done = 0;
    if (!report(progress, done, range.length(), error))
        return false;
    while (done < range.length()) {
        QByteArray data;
        if (!readBlock(file, range.length() - done, data, error))
            return false;
        calculator.addData(data);
        done += data.size();
        if (!report(progress, done, range.length(), error))
            return false;
    }
    result = calculator.result();
    return true;
}

bool AppFileTools::splitFile(const QString &source, const QString &destination, const Range &range,
                             QString &error, const Progress &progress) {
    error.clear();
    if (destination.trimmed().isEmpty()) {
        error = QStringLiteral("请填写保存路径。");
        return false;
    }
    QFile input(source);
    if (!prepare(input, range, error))
        return false;
    const QFileInfo src(source), dst(destination);
    if (src.canonicalFilePath().compare(dst.canonicalFilePath(), Qt::CaseInsensitive) == 0 ||
        src.absoluteFilePath().compare(dst.absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        error = QStringLiteral("保存路径不能与源文件相同。");
        return false;
    }
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("无法创建目标文件：%1").arg(output.errorString());
        return false;
    }
    qint64 done = 0;
    if (!report(progress, done, range.length(), error))
        return false;
    while (done < range.length()) {
        QByteArray data;
        if (!readBlock(input, range.length() - done, data, error))
            return false;
        if (output.write(data) != data.size()) {
            error = QStringLiteral("目标文件写入不完整：%1").arg(output.errorString());
            return false;
        }
        done += data.size();
        if (!report(progress, done, range.length(), error))
            return false;
    }
    if (!output.commit()) {
        error = QStringLiteral("目标文件保存失败：%1").arg(output.errorString());
        return false;
    }
    return true;
}
