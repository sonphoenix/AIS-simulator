#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ais_sim/ais_message.hpp"

namespace ais_sim {

    class AisClient {
    public:
        using MessageHandler = std::function<void(const AisMessage&)>;

        // bounding_box is [[lat1, lon1], [lat2, lon2]] describing the
        // rectangle of ocean you want traffic for. aisstream.io requires one -
        AisClient(std::string url, std::string api_key,
            double lat1, double lon1, double lat2, double lon2);
        ~AisClient();

        void on_message(MessageHandler handler);
        void connect();
        void run();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;

        std::string build_subscription_message() const;
    };

}  // namespace ais_sim