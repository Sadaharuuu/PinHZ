#include "AppNavigation.h"
#include <cmath>
namespace navigation {
namespace {
const double a = 6378137.0, f = 1.0 / 298.257223563;
const double e2 = f * (2 - f), rad = 3.14159265358979323846 / 180;
bool finite(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
} // namespace
bool toEcef(Vec3 b, Vec3 &v) {
    if (!finite(b) || std::abs(b.x) > 90 || std::abs(b.y) > 180)
        return false;
    const double lat = b.x * rad, lon = b.y * rad, s = std::sin(lat), c = std::cos(lat);
    const double n = a / std::sqrt(1 - e2 * s * s);
    v = {(n + b.z) * c * std::cos(lon), (n + b.z) * c * std::sin(lon), (n * (1 - e2) + b.z) * s};
    return finite(v);
}
bool toGeodetic(Vec3 v, Vec3 &b) {
    if (!finite(v))
        return false;
    const double p = std::hypot(v.x, v.y);
    // Only the geocentric origin has no defined latitude / longitude.
    if (std::hypot(p, v.z) < 1e-8)
        return false;
    if (p < 1e-8) {
        b = {v.z >= 0 ? 90.0 : -90.0, 0, std::abs(v.z) - a * (1 - f)};
        return finite(b);
    }
    double lat = std::atan2(v.z, p * (1 - e2));
    bool converged = false;
    for (int i = 0; i < 32; ++i) {
        const double s = std::sin(lat), n = a / std::sqrt(1 - e2 * s * s);
        const double next = std::atan2(v.z + e2 * n * s, p);
        if (std::abs(next - lat) < 1e-14) {
            lat = next;
            converged = true;
            break;
        }
        lat = next;
    }
    if (!converged)
        return false;
    const double s = std::sin(lat), c = std::cos(lat), n = a / std::sqrt(1 - e2 * s * s);
    // Project onto the ellipsoid normal; stable at the poles as well as equator.
    b = {lat / rad, std::atan2(v.y, v.x) / rad, p * c + v.z * s - n * (1 - e2 * s * s)};
    return finite(b);
}
bool localFrame(Vec3 origin, double r[9]) {
    Vec3 b;
    if (!toGeodetic(origin, b))
        return false;
    const double s = std::sin(b.x * rad), c = std::cos(b.x * rad), sl = std::sin(b.y * rad),
                 cl = std::cos(b.y * rad);
    const double values[9] = {-sl, cl, 0, -s * cl, -s * sl, c, c * cl, c * sl, s};
    for (int i = 0; i < 9; ++i)
        r[i] = values[i];
    return true;
}
Vec3 rotate(const double r[9], Vec3 v, bool inverse) {
    if (inverse)
        return {r[0] * v.x + r[3] * v.y + r[6] * v.z, r[1] * v.x + r[4] * v.y + r[7] * v.z,
                r[2] * v.x + r[5] * v.y + r[8] * v.z};
    return {r[0] * v.x + r[1] * v.y + r[2] * v.z, r[3] * v.x + r[4] * v.y + r[5] * v.z,
            r[6] * v.x + r[7] * v.y + r[8] * v.z};
}
} // namespace navigation

#include <QFile>
#include <QtEndian>
namespace navigation {
bool geoid(double lat, double lon, double &n) {
    if (!std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90)
        return false;
    static const QByteArray grid = [] {
        QFile f(":/navigation/EGM.dat");
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        return f.readAll();
    }();
    if (grid.size() != 1441 * 721 * 2)
        return false;
    lon = std::fmod(lon, 360.0);
    if (lon < 0)
        lon += 360;
    const double x = lon * 4, y = (90 - lat) * 4;
    int ix = qMin(1439, int(std::floor(x))), iy = qMin(719, int(std::floor(y)));
    const double dx = x - ix, dy = y - iy;
    auto at = [&](int r, int c) {
        return double(qFromLittleEndian<qint16>(
                   reinterpret_cast<const uchar *>(grid.constData() + 2 * (r * 1441 + c)))) *
               0.01;
    };
    n = (1 - dy) * ((1 - dx) * at(iy, ix) + dx * at(iy, ix + 1)) +
        dy * ((1 - dx) * at(iy + 1, ix) + dx * at(iy + 1, ix + 1));
    return std::isfinite(n);
}
} // namespace navigation
