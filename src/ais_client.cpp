#include "ais_sim/ais_client.hpp"

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace ais_sim {

    using json = nlohmann::json;

    struct AisClient::Impl {
        std::string url;
        std::string api_key;
        double lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
        ix::WebSocket socket;
        MessageHandler handler;
    };

    AisClient::AisClient(std::string url, std::string api_key,
        double lat1, double lon1, double lat2, double lon2)
        : impl_(std::make_unique<Impl>()) {
        impl_->url = std::move(url);
        impl_->api_key = std::move(api_key);
        impl_->lat1 = lat1;
        impl_->lon1 = lon1;
        impl_->lat2 = lat2;
        impl_->lon2 = lon2;
    }

    AisClient::~AisClient() {
        impl_->socket.stop();
    }

    void AisClient::on_message(MessageHandler handler) {
        impl_->handler = std::move(handler);
    }

    std::string AisClient::build_subscription_message() const {
        json msg;
        msg["APIKey"] = impl_->api_key;
        msg["BoundingBoxes"] = json::array({
            json::array({
                json::array({impl_->lat1, impl_->lon1}),
                json::array({impl_->lat2, impl_->lon2})
            })
            });
        return msg.dump();
    }

    void AisClient::connect() {
        impl_->socket.setUrl(impl_->url);

        impl_->socket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                std::string subscription = build_subscription_message();
                impl_->socket.send(subscription);
                std::cout << "Sent subscription, waiting for ship data...\n";
                return;
            }

            if (msg->type == ix::WebSocketMessageType::Error) {
                std::cerr << "WebSocket error: " << msg->errorInfo.reason << "\n";
                return;
            }

            if (msg->type != ix::WebSocketMessageType::Message) {
                return;
            }

            try {
                auto payload = json::parse(msg->str);

                std::string message_type = payload.value("MessageType", "");
                if (message_type != "PositionReport") {
                    return;  // Not a position update - ShipStaticData, etc. Skip quietly.
                }

                auto& report = payload.at("Message").at("PositionReport");

                AisMessage parsed;
                parsed.mmsi = report.at("UserID").get<uint32_t>();
                parsed.lat = report.at("Latitude").get<double>();
                parsed.lon = report.at("Longitude").get<double>();
                parsed.sog_knots = report.at("Sog").get<double>();
                parsed.cog_deg = report.at("Cog").get<double>();

                if (payload.contains("MetaData")) {
                    auto& meta = payload["MetaData"];
                    if (meta.contains("time_utc")) {
                        parsed.timestamp_utc = meta["time_utc"].get<std::string>();
                    }
                    if (meta.contains("ShipName")) {
                        parsed.ship_name = meta["ShipName"].get<std::string>();
                    }
                }

                if (impl_->handler) {
                    impl_->handler(parsed);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to parse AIS message: " << e.what() << "\n";
            }
            });
    }

    void AisClient::run() {
        impl_->socket.start();
      /*  while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        */
    }

}  