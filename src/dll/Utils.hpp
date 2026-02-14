#pragma once
#include <cstdint>

inline uint64_t MakeGIKey(const uint32_t groupId, const uint32_t instanceId) {
    return (static_cast<uint64_t>(groupId) << 32) | instanceId;
}
