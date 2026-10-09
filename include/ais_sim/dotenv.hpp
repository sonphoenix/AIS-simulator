#pragma once

#include <string>

namespace ais_sim {

	// Very small .env file reader. Looks for a line like KEY=value and
	// returns value as a string, or an empty string if not found / file
	// doesn't exist. Not a general-purpose .env parser - just enough for
	// our one API key.
	std::string read_env_value(const std::string& file_path, const std::string& key);

}  // namespace ais_sim