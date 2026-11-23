#ifndef APPNAVIGATION_H
#define APPNAVIGATION_H
namespace navigation {
struct Vec3 {
    double x, y, z;
};
// WGS84 only. Geodetic order: latitude(deg), longitude(deg), ellipsoidal height(m).
bool toGeodetic(Vec3 xyz, Vec3 &blh);
bool toEcef(Vec3 blh, Vec3 &xyz);
bool localFrame(Vec3 origin, double rotation[9]);
Vec3 rotate(const double r[9], Vec3 v, bool inverse = false);
bool geoid(double latitudeDegrees, double longitudeDegrees, double &n);
} // namespace navigation
#endif
