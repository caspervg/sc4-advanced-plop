#include "ExemplarParser.hpp"

std::optional<ExemplarType> ExemplarParser::GetExemplarType(const Exemplar::Record& exemplar) const {
    auto* prop = exemplar.FindProperty(propertyMapper_.GetPropertyId(kExemplarType).value());
    if (!prop || prop->values.empty()) {
        // Unknown or empty Exemplar Type
        return std::nullopt;
    }

    const auto exemplarType = std::get<uint32_t>(prop->values.front());
    if (exemplarType == propertyMapper_.GetPropertyOptionId(kExemplarType, kExemplarTypeBuilding)) return ExemplarType::Building;
    if (exemplarType == propertyMapper_.GetPropertyOptionId(kExemplarType, kExemplarTypeLotConfig)) return ExemplarType::LotConfig;
    return std::nullopt;
}

std::optional<ParsedBuildingExemplar> ExemplarParser::ParseBuilding(const DBPF::Reader& reader, const DBPF::IndexEntry& entry) const {
    auto exemplar = reader.LoadExemplar(entry);
    if (!exemplar) return std::nullopt;

    ParsedBuildingExemplar parsedBuildingExemplar;
    parsedBuildingExemplar.tgi = entry.tgi;

    if (auto* prop = exemplar->FindProperty(propertyMapper_.GetPropertyId(kExemplarName).value())) {
        if (prop->IsString() && !prop->values.empty()) {
            parsedBuildingExemplar.name = std::get<std::string>(prop->values.front());
        }
    }

    if (auto* prop = exemplar->FindProperty(propertyMapper_.GetPropertyId(kOccupantGroups).value())) {
        if (prop->IsUint32Array()) {
            for (const auto& val : prop->values) {
                const auto& arr = std::get<std::vector<uint32_t>>(val);
                parsedBuildingExemplar.occupantGroups.insert(
                    parsedBuildingExemplar.occupantGroups.end(),
                    arr.begin(),
                    arr.end()
                );
            }
        }
    }

    return parsedBuildingExemplar;
}

std::optional<ParsedLotConfigExemplar> ExemplarParser::ParseLotConfig(const DBPF::Reader& reader, const DBPF::IndexEntry& entry) const {
    auto exemplar = reader.LoadExemplar(entry);
    if (!exemplar) return std::nullopt;

    ParsedLotConfigExemplar parsedLotConfigExemplar;
    parsedLotConfigExemplar.tgi = entry.tgi;

    if (auto* prop = exemplar->FindProperty(propertyMapper_.GetPropertyId(kExemplarName).value())) {
        if (prop->IsString() && !prop->values.empty()) {
            parsedLotConfigExemplar.name = std::get<std::string>(prop->values.front());
        }
    }

    if (auto* prop = exemplar->FindProperty(propertyMapper_.GetPropertyId(kLotConfigSize).value())) {
        if (prop->IsList() && prop->values.size() >= 2) {
            uint8_t width = std::get<uint8_t>(prop->values[0]);
            uint8_t height = std::get<uint8_t>(prop->values[1]);
            parsedLotConfigExemplar.lotSize = {width, height};
        }
    }

    std::vector<Exemplar::Property> objectProperties{};
    parsedLotConfigExemplar.buildingInstanceId = -1;
    if (exemplar->FindProperties(propertyMapper_.GetPropertyId(kLotConfigObject).value(), &objectProperties)) {
        for (auto const& prop : objectProperties) {
            if (prop.values.size() >= 13) {
                const auto objectType = std::get<uint32_t>(prop.values[0]);
                if (objectType == kLotConfigObjectTypeBuilding) {
                    parsedLotConfigExemplar.buildingInstanceId = std::get<uint32_t>(prop.values[12]);
                    break;
                }
            }
        }
    }
    if (parsedLotConfigExemplar.buildingInstanceId == -1) {
        // We encountered a lot without a valid building instance, skipping!
        return std::nullopt;
    }

    const auto growthStage = exemplar->GetScalar(propertyMapper_.GetPropertyId(kGrowthStage).value());
    if (growthStage.has_value()) {
        parsedLotConfigExemplar.growthStage = growthStage.value();
    }

    // TODO: Implement map parsing for Capacity Satisfied (its a map)

    if (auto* prop = exemplar->FindProperty(propertyMapper_.GetPropertyId(kIconResourceKey).value())) {
        // I'm fairly certain Icon Resource Key is not typically used in Lot Configurations, but adding it just in case
        if (prop->values.size() >= 3) {
            parsedLotConfigExemplar.iconTgi = DBPF::Tgi{
                std::get<uint32_t>(prop->values[0]),
                std::get<uint32_t>(prop->values[1]),
                std::get<uint32_t>(prop->values[2])
            };
        }
    } else {
        const auto iconInstance = exemplar->GetScalar(propertyMapper_.GetPropertyId(kItemIcon));
        if (iconInstance.has_value()) {
            parsedLotConfigExemplar.iconTgi = DBPF::Tgi{
                kTypeIdPNG,
                kZero,
                iconInstance.value()
            };
        }
    }

    return parsedLotConfigExemplar;
}