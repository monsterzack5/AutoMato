#pragma once

#include <fmt/format.h>
#include <fstream>
#include <string>

enum class ENV {
    NODE_RED_DIR,
    DB_FILE,
};

inline std::string get_env_var(const ENV var)
{

    auto EnvVar_to_key = [](const ENV var) {
        switch (var) {
        case ENV::NODE_RED_DIR:
            return "userDir";
        case ENV::DB_FILE:
            return "dbFile";

        default:
            __builtin_unreachable();
        }
    };

    std::ifstream file("./.env");

    const std::string key_to_search_for = EnvVar_to_key(var);

    for (std::string line; std::getline(file, line);) {
        const size_t seperator_position = line.find('=');

        if (seperator_position == std::string::npos) {
            // We couldn't find a seperator
            continue;
        }

        const std::string found_key = line.substr(0, seperator_position);
        const std::string found_value = line.substr(seperator_position + 1, line.length());

        if (found_key == key_to_search_for) {
            return found_value;
        }
    }

    // we couldn't find the value, this is a fatal error.
    fmt::print("Error: Unable to find {} in .env file.\n", EnvVar_to_key(var));
    exit(1);
    return "";
}