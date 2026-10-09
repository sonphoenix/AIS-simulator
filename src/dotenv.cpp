#include "ais_sim/dotenv.hpp"

#include <fstream>
#include <sstream>

namespace ais_sim {

    std::string read_env_value(const std::string& file_path, const std::string& key) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            return "";
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip blank lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            auto equals_pos = line.find('=');
            if (equals_pos == std::string::npos) {
                continue;
            }

            std::string line_key = line.substr(0, equals_pos);
            std::string line_value = line.substr(equals_pos + 1);

            if (line_key == key) {
                return line_value;
            }
        }

        return "";
    }

}  // namespace ais_sim