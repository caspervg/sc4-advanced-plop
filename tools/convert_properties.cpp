#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <rfl/Hex.hpp>
#include <rfl/Rename.hpp>
#include <rfl/json.hpp>
#include <rfl/xml.hpp>
#include "../src/shared/properties.hpp"

// Structures matching the new_properties.xml format
// <ExemplarProperties>
//   <PROPERTIES>
//     <PROPERTY ID="..." Name="...">
//       <OPTION Value="..." Name="..."/>
//     </PROPERTY>
//   </PROPERTIES>
// </ExemplarProperties>

namespace XmlFormat {

struct Option {
    rfl::Rename<"Value", std::string> value;  // Parse as string - can be hex, decimal, or symbolic
    rfl::Rename<"Name", std::string> name;
};

struct Property {
    rfl::Rename<"ID", std::string> id;
    rfl::Rename<"Name", std::string> name;
    rfl::Rename<"OPTION", std::vector<Option>> options;
};

struct Properties {
    rfl::Rename<"PROPERTY", std::vector<Property>> properties;
};

struct ExemplarProperties {
    rfl::Rename<"PROPERTIES", Properties> properties;
};

} // namespace XmlFormat

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <input.xml> [output.json]\n";
            return 1;
        }

        std::filesystem::path inputPath = argv[1];
        std::filesystem::path outputPath = argc >= 3 ? argv[2] : "properties.json";

        std::cout << "Converting " << inputPath << " to " << outputPath << "...\n";

        // Read file content
        std::ifstream file(inputPath);
        if (!file) {
            throw std::runtime_error("Failed to open input file");
        }
        std::string xmlContent((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        file.close();

        std::cout << "File size: " << xmlContent.size() << " bytes\n";

        // Read XML using reflect-cpp
        auto xmlData = rfl::xml::read<XmlFormat::ExemplarProperties>(xmlContent);
        if (!xmlData) {
            std::cerr << "XML parsing failed: " << xmlData.error().what() << "\n";
            throw std::runtime_error("Failed to parse XML: " + xmlData.error().what());
        }

        std::cout << "XML parsed successfully\n";
        std::cout << "Properties found: " << xmlData->properties().properties().size() << "\n";

        // Helper to parse hex string (property IDs)
        auto parseHex = [](const std::string& hex) -> std::optional<uint32_t> {
            if (hex.empty()) return std::nullopt;

            try {
                // Remove any whitespace
                std::string cleaned = hex;
                cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());

                if (cleaned.empty()) return std::nullopt;

                // Parse as hex
                return static_cast<uint32_t>(std::stoul(cleaned, nullptr, 16));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Failed to parse property ID '" << hex << "': " << e.what() << "\n";
                return std::nullopt;
            }
        };

        // Helper to parse option value (can be hex, decimal, or symbolic)
        auto parseOptionValue = [](const std::string& value) -> std::optional<uint32_t> {
            if (value.empty()) return std::nullopt;

            // Skip symbolic values like "Col:0"
            if (value.find(':') != std::string::npos) {
                return std::nullopt;
            }

            // Remove whitespace
            std::string cleaned = value;
            cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());

            if (cleaned.empty()) return std::nullopt;

            try {
                // Try to parse as hex (0x prefix) or decimal
                if (cleaned.find("0x") == 0 || cleaned.find("0X") == 0) {
                    return static_cast<uint32_t>(std::stoul(cleaned, nullptr, 16));
                } else {
                    return static_cast<uint32_t>(std::stoul(cleaned, nullptr, 10));
                }
            } catch (const std::exception& e) {
                // Silently skip unparseable values
                return std::nullopt;
            }
        };

        // Convert to simplified format
        PropertiesData data;
        for (const auto& xmlProp : xmlData->properties().properties()) {
            auto propId = parseHex(xmlProp.id());
            if (!propId) {
                // Skip properties with unparseable IDs
                continue;
            }

            PropertyDef prop;
            prop.id = rfl::Hex(*propId);
            prop.name = xmlProp.name();

            for (const auto& xmlOpt : xmlProp.options()) {
                auto optValue = parseOptionValue(xmlOpt.value());
                if (optValue) {
                    PropertyOption opt;
                    opt.value = rfl::Hex(*optValue);
                    opt.name = xmlOpt.name();
                    prop.options.push_back(std::move(opt));
                }
                // Skip symbolic/unparseable options
            }

            data.properties.push_back(std::move(prop));
        }

        std::cout << "Converted " << data.properties.size() << " properties\n";
        if (!data.properties.empty()) {
            std::cout << "Sample property: ID=0x" << std::hex << data.properties[0].id.get()
                      << std::dec << " Name=" << data.properties[0].name << "\n";
        }

        // Write JSON with pretty printing
        std::ofstream output(outputPath);
        if (!output) {
            throw std::runtime_error("Failed to open output: " + outputPath.string());
        }

        rfl::json::write(data, output);
        output.close();

        std::cout << "Conversion complete!\n";
        std::cout << "Output written to: " << outputPath << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
