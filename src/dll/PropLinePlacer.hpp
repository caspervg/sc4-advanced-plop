#pragma once

#include <cstddef>
#include <vector>

#include "PropPaintPlacement.hpp"

class cISTETerrain;

class PropLinePlacer {
public:
    static std::vector<PlannedProp> ComputePlacements(
        const std::vector<cS3DVector3>& linePoints,
        float spacingMeters,
        int32_t baseRotation,
        bool alignToPath,
        float randomOffset,
        cISTETerrain* terrain,
        uint32_t seed,
        size_t maxPlacements = static_cast<size_t>(-1));
};
