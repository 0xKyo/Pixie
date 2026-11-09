#pragma once

#include <SDL3/SDL.h>

#include <map>
#include <string>

#include "core/library/Asset.h"

namespace pixie::services {

struct Thumbnail {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    bool failed = false; // the file couldn't be decoded; don't retry
};

// Lazily decoded thumbnails for the library grid, rationed per frame so scrolling stays smooth.
class ThumbnailCache {
public:
    ThumbnailCache() = default;
    ~ThumbnailCache() { Clear(); }
    ThumbnailCache(const ThumbnailCache&) = delete;
    ThumbnailCache& operator=(const ThumbnailCache&) = delete;

    void BeginFrame(); // call once per frame before Get()

    // The asset's thumbnail; returns an empty one (no texture, not failed) while it's still pending.
    const Thumbnail& Get(SDL_Renderer* renderer, const library::Asset& asset);

    void Forget(const std::string& assetKey); // drops one asset's thumbnails
    void Clear();                             // frees every texture; call before the renderer dies

private:
    std::map<std::string, Thumbnail> cache_; // key: asset key + "|" + file path
    Uint64 frameStartMs_ = 0;
    int decodedThisFrame_ = 0;
};

} // namespace pixie::services
