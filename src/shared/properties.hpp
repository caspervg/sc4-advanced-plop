#pragma once

#include <cstdint>
#include <string>
#include "rfl/Hex.hpp"

// Property option (enum value)
struct PropertyOption {
    rfl::Hex<uint32_t> value;
    std::string name;
};

// Property definition
struct PropertyDef {
    rfl::Hex<uint32_t> id;
    std::string name;
    std::vector<PropertyOption> options;  // Empty if no options
};

// Root structure for the properties file
struct PropertiesData {
    std::vector<PropertyDef> properties;
};
