#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ais_sim/ais_message.hpp"
#include "ais_sim/coordinate_converter.hpp"
namespace ais_sim {

    struct ShipState {
        uint32_t mmsi = 0;
        double posX = 0.0;
        double posZ = 0.0;
        double sog_knots = 0.0;
        double cog_deg = 0.0;
        std::string ship_name;
        std::string last_update_utc;
    };

    class ShipRegistry {
    public:
        void update(const AisMessage& msg);
        std::vector<ShipState> snapshot() const;
        static constexpr double lat = 37.5;
        static constexpr double lon = -122.5;

    private:
        ais_sim::CoordinateConverter converter{ lat,lon };
        mutable std::mutex mutex_;
        std::unordered_map<uint32_t, ShipState> ships_;
    };

}  // namespace ais_sim