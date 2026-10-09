#include "ais_sim/coordinate_converter.hpp"

#include <cmath>

namespace ais_sim {

    namespace {
        constexpr double kMetersPerDegreeLat = 111320.0;
        constexpr double kPi = 3.14159265358979323846;

        double degrees_to_radians(double degrees) {
            return degrees * kPi / 180.0;
        }
    }  // namespace

    CoordinateConverter::CoordinateConverter(double anchor_lat, double anchor_lon)
        : anchor_lat_(anchor_lat), anchor_lon_(anchor_lon) {
        meters_per_degree_lat_ = kMetersPerDegreeLat;
        meters_per_degree_lon_ = kMetersPerDegreeLat * std::cos(degrees_to_radians(anchor_lat_));
    }

    LocalPosition CoordinateConverter::convert(double lat, double lon) const {
        LocalPosition result;
        result.x = (lon - anchor_lon_) * meters_per_degree_lon_;
        result.z = (lat - anchor_lat_) * meters_per_degree_lat_;
        return result;
    }

}  // namespace ais_sim