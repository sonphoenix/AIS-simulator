#include "ais_sim/ship_registry.hpp"
namespace ais_sim {

    void ShipRegistry::update(const AisMessage& msg) {
        std::lock_guard<std::mutex> lock(mutex_);

        ShipState& state = ships_[msg.mmsi];
        LocalPosition pos = converter.convert(msg.lat, msg.lon);
        state.mmsi = msg.mmsi;
        state.posX =pos.x;
        state.posZ = pos.z;
        state.sog_knots = msg.sog_knots;
        state.cog_deg = msg.cog_deg;
        state.ship_name = msg.ship_name;
        state.last_update_utc = msg.timestamp_utc;
    }

    std::vector<ShipState> ShipRegistry::snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<ShipState> result;
        result.reserve(ships_.size());
        for (const auto& [mmsi, state] : ships_) {
            result.push_back(state);
        }
        return result;
    }

}  // namespace ais_sim