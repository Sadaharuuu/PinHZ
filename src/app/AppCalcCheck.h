#ifndef APPCALCCHECK_H
#define APPCALCCHECK_H

#include "AppCalcCRC.h"
#include "third_party/blake2/blake2.h"
#include "third_party/xxhash/xxhash.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QString>

// The same incremental calculator is used for text bytes and binary file ranges.
class AppCalcCheck {
public:
    // Order matches the shared algorithm selector; these indexes are not stored in frame templates.
    enum Type {
        Sum8,
        Sum16,
        Sum32,
        CRC,
        Sha224,
        Sha256,
        Sha384,
        Sha512,
        Md5,
        Sha1,
        Md4,
        Sha3_224,
        Sha3_256,
        Sha3_384,
        Sha3_512,
        Adler32,
        Xor8,
        Blake2sp256,
        Blake2s256,
        Blake2b512,
        Xxh32,
        Xxh64,
        Xxh3_64,
        Xxh3_128,
        Crc64Ecma,
        Crc64Xz,
        Lrc8,
        Fletcher16,
        Internet16,
        TypeCount
    };
    static bool isDigest(Type type) {
        return (type >= Sha224 && type <= Sha3_512) || (type >= Blake2sp256 && type <= Xxh3_128);
    }
    struct Settings {
        Type type = Sum8;
        s_crc_conf crc = {};
        bool littleEndian = false;
    };

    explicit AppCalcCheck(const Settings &settings);
    ~AppCalcCheck();
    bool isValid() const;
    void addData(const QByteArray &data);
    QByteArray result() const;
    static bool parseHex(const QString &text, QByteArray &data, QString &error);
    static QByteArray calculate(const QByteArray &data, Type type, const s_crc_conf &crc,
                                bool littleEndian = false);

private:
    Settings m_settings;
    quint32 m_value;
    quint32 m_crcTable[256] = {};
    QCryptographicHash m_hash;
    quint64 m_wideValue = 0;
    bool m_pendingByte = false;
    quint8 m_lastByte = 0;
    XXH32_state_t *m_xxh32 = nullptr;
    XXH64_state_t *m_xxh64 = nullptr;
    XXH3_state_t *m_xxh3 = nullptr;
    blake2sp_state m_blakeSp = {};
    blake2s_state m_blakeS = {};
    blake2b_state m_blakeB = {};
};

#endif
