#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "Constants.hpp"
#include "public/ImGuiTexture.h"

/**
 * LRU-based thumbnail cache with configurable capacity and load rate limiting.
 *
 * Features:
 * - Automatic LRU eviction when capacity is exceeded
 * - Deferred loading via request queue (to spread GPU uploads across frames)
 * - Device reset handling (clears all textures)
 *
 * Template parameter KeyType should be hashable (e.g., uint32_t, uint64_t).
 */
template<typename KeyType>
class ThumbnailCache {
public:
    explicit ThumbnailCache(
        size_t maxSize = ThumbnailCacheConfig::kMaxCacheSize,
        size_t maxLoadPerFrame = ThumbnailCacheConfig::kMaxLoadPerFrame
    ) : maxSize_(maxSize), maxLoadPerFrame_(maxLoadPerFrame) {}

    ~ThumbnailCache() = default;

    // Non-copyable
    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    // Movable
    ThumbnailCache(ThumbnailCache&&) = default;
    ThumbnailCache& operator=(ThumbnailCache&&) = default;

    /**
     * Gets a texture from the cache. Returns nullptr if not present.
     * Updates the LRU timestamp if found.
     */
    void* Get(const KeyType& key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return nullptr;
        }

        // Move to front of LRU list (most recently used)
        lruList_.splice(lruList_.begin(), lruList_, it->second.lruIter);

        return it->second.texture.GetID();
    }

    /**
     * Checks if a key exists in the cache (without updating LRU).
     */
    [[nodiscard]] bool Contains(const KeyType& key) const {
        return cache_.contains(key);
    }

    /**
     * Inserts a texture into the cache. Evicts LRU entries if over capacity.
     * Takes ownership of the texture via move.
     */
    void Insert(const KeyType& key, ImGuiTexture texture) {
        // If key already exists, update it
        auto existingIt = cache_.find(key);
        if (existingIt != cache_.end()) {
            existingIt->second.texture = std::move(texture);
            lruList_.splice(lruList_.begin(), lruList_, existingIt->second.lruIter);
            return;
        }

        // Evict if necessary
        while (cache_.size() >= maxSize_ && !lruList_.empty()) {
            const KeyType& evictKey = lruList_.back();
            cache_.erase(evictKey);
            lruList_.pop_back();
        }

        // Insert new entry
        lruList_.push_front(key);
        CacheEntry entry;
        entry.texture = std::move(texture);
        entry.lruIter = lruList_.begin();
        cache_.emplace(key, std::move(entry));
    }

    /**
     * Requests a key to be loaded. Keys are deduplicated and processed
     * in subsequent ProcessLoadQueue() calls.
     */
    void RequestLoad(const KeyType& key) {
        // Don't request if already in cache or already pending
        if (cache_.contains(key) || pendingLoads_.contains(key)) {
            return;
        }
        loadQueue_.push_back(key);
        pendingLoads_.insert(key);
    }

    /**
     * Processes the load queue, calling the loader callback for up to
     * maxLoadPerFrame items. The loader should return an ImGuiTexture
     * (possibly invalid if loading failed).
     *
     * Loader signature: ImGuiTexture loader(const KeyType& key)
     */
    template<typename LoaderFunc>
    void ProcessLoadQueue(LoaderFunc&& loader) {
        size_t loaded = 0;
        while (!loadQueue_.empty() && loaded < maxLoadPerFrame_) {
            const KeyType key = loadQueue_.front();
            loadQueue_.pop_front();
            pendingLoads_.erase(key);

            // Skip if already loaded (could happen due to race)
            if (cache_.contains(key)) {
                continue;
            }

            ImGuiTexture texture = loader(key);
            if (texture.GetHandle().id != 0) {
                Insert(key, std::move(texture));
            }
            ++loaded;
        }
    }

    /**
     * Clears all cached textures. Call this on device reset.
     */
    void OnDeviceReset() {
        cache_.clear();
        lruList_.clear();
        loadQueue_.clear();
        pendingLoads_.clear();
    }

    /**
     * Returns the current number of cached textures.
     */
    [[nodiscard]] size_t Size() const {
        return cache_.size();
    }

    /**
     * Returns the maximum cache capacity.
     */
    [[nodiscard]] size_t MaxSize() const {
        return maxSize_;
    }

    /**
     * Checks if the load queue is empty.
     */
    [[nodiscard]] bool IsLoadQueueEmpty() const {
        return loadQueue_.empty();
    }

private:
    struct CacheEntry {
        ImGuiTexture texture;
        typename std::list<KeyType>::iterator lruIter;
    };

    size_t maxSize_;
    size_t maxLoadPerFrame_;

    // LRU list: front = most recently used, back = least recently used
    std::list<KeyType> lruList_;

    // Key -> CacheEntry mapping
    std::unordered_map<KeyType, CacheEntry> cache_;

    // Load queue (FIFO) and deduplication set
    std::list<KeyType> loadQueue_;
    std::unordered_set<KeyType> pendingLoads_;
};
