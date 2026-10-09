#pragma once

namespace ais_sim {

    struct LocalPosition {
        double x = 0.0;  // meters east(+)/west(-) of anchor
        double z = 0.0;  // meters north(+)/south(-) of anchor
    };

    class CoordinateConverter {
    public:
        CoordinateConverter( double anchor_lat, double anchor_lon);

        LocalPosition convert(double lat, double lon) const;

    private:
        double anchor_lat_;
        double anchor_lon_;
        double meters_per_degree_lat_;
        double meters_per_degree_lon_;
    };

}  // namespace ais_sim