#include "app/services/ThumbnailCache.h"

#include "app/util/Texture.h"
#include "core/formats/ImageFile.h"
#include "core/graphics/Resize.h"

namespace pixie::services {

namespace {

constexpr int kMaxThumbnailSide = 256;
constexpr Uint64 kDecodeBudgetMs = 12; // after the first decode, stop once a frame has used this much

} // namespace

void ThumbnailCache::BeginFrame() {
    frameStartMs_ = SDL_GetTicks();
    decodedThisFrame_ = 0;
}

const Thumbnail& ThumbnailCache::Get(SDL_Renderer* renderer, const library::Asset& asset) {
    static const Thumbnail kPending;
    const std::filesystem::path& file = asset.MainFile();
    const std::string cacheKey = asset.key + "|" + file.generic_string();
    if (auto cached = cache_.find(cacheKey); cached != cache_.end()) {
        return cached->second;
    }
    if (decodedThisFrame_ >= 1 && SDL_GetTicks() - frameStartMs_ > kDecodeBudgetMs) {
        return kPending;
    }
    ++decodedThisFrame_;

    Thumbnail thumbnail;
    graphics::Image fullSize;
    if (formats::LoadImageFile(file, fullSize)) {
        const bool wasShrunk = fullSize.Width() > kMaxThumbnailSide || fullSize.Height() > kMaxThumbnailSide;
        const graphics::Image small = graphics::MakeThumbnail(fullSize, kMaxThumbnailSide);
        thumbnail.texture = util::CreateTexture(renderer, small);
        if (thumbnail.texture && wasShrunk) {
            SDL_SetTextureScaleMode(thumbnail.texture, SDL_SCALEMODE_LINEAR); // hi-res art looks better smoothed
        }
        thumbnail.width = small.Width();
        thumbnail.height = small.Height();
    }
    thumbnail.failed = thumbnail.texture == nullptr;
    return cache_.emplace(cacheKey, thumbnail).first->second;
}

void ThumbnailCache::Forget(const std::string& assetKey) {
    const std::string prefix = assetKey + "|";
    for (auto entry = cache_.begin(); entry != cache_.end();) {
        if (entry->first.rfind(prefix, 0) == 0) {
            util::DestroyTexture(entry->second.texture);
            entry = cache_.erase(entry);
        } else {
            ++entry;
        }
    }
}

void ThumbnailCache::Clear() {
    for (auto& [key, thumbnail] : cache_) {
        util::DestroyTexture(thumbnail.texture);
    }
    cache_.clear();
}

} // namespace pixie::services
