#pragma once

#include <cstdint>
#include <string>

namespace ais_sim {

    struct AisMessage {
        uint32_t mmsi = 0;
        double lat = 0.0;
        double lon = 0.0;
        double sog_knots = 0.0;
        double cog_deg = 0.0;
        std::string ship_name;
        std::string timestamp_utc;
    };

}  