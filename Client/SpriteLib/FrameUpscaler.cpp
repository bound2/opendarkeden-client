#include "FrameUpscaler.h"
#include "xbrz.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <array>
#include <future>
#include <thread>

int FrameUpscaler::ScaleFactor(int width, int height, const SDL_Rect& destination)
{
    if (width <= 0 || height <= 0 || destination.w <= 0 || destination.h <= 0) return 0;
    const auto x = (static_cast<long long>(destination.w) + width - 1) / width;
    const auto y = (static_cast<long long>(destination.h) + height - 1) / height;
    return static_cast<int>(std::clamp(std::max(x, y), 2LL, 4LL));
}

void FrameUpscaler::Release(SDL_Renderer* renderer)
{
    if (renderer && renderer != renderer_) return;
    if (texture_) SDL_DestroyTexture(texture_);
    texture_ = nullptr;
    renderer_ = nullptr;
    width_ = height_ = factor_ = 0;
    valid_ = false;
    std::vector<uint32_t>().swap(current_);
    std::vector<uint32_t>().swap(previous_);
    std::vector<uint32_t>().swap(output_);
}

bool FrameUpscaler::Draw(SDL_Surface* source, SDL_Renderer* renderer, const SDL_Rect& destination)
{
    if (!source || !renderer) return false;
    const int factor = ScaleFactor(source->w, source->h, destination);
    if (!factor) return false;
    const auto width = static_cast<long long>(source->w) * factor;
    const auto height = static_cast<long long>(source->h) * factor;
    // Bound the intermediate image to 64 MiB, and respect GPU texture limits.
    SDL_RendererInfo info{};
    if (width > 16384 || height > 16384 || width * height > 16LL * 1024 * 1024 ||
        SDL_GetRendererInfo(renderer, &info) != 0 ||
        (info.max_texture_width > 0 && width > info.max_texture_width) ||
        (info.max_texture_height > 0 && height > info.max_texture_height)) return false;

    if (renderer_ != renderer || width_ != source->w || height_ != source->h || factor_ != factor) {
        Release();
        renderer_ = renderer;
        width_ = source->w;
        height_ = source->h;
        factor_ = factor;
        texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
            SDL_TEXTUREACCESS_STREAMING, static_cast<int>(width), static_cast<int>(height));
        if (!texture_) return false;
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
    }
    if (!texture_) return false;

    try {
        current_.resize(static_cast<size_t>(source->w) * source->h);
        const bool lock = SDL_MUSTLOCK(source) != 0;
        if (lock && SDL_LockSurface(source) != 0) return false;
        const int converted = SDL_ConvertPixels(source->w, source->h,
            source->format->format, source->pixels, source->pitch,
            SDL_PIXELFORMAT_RGB888, current_.data(), source->w * sizeof(uint32_t));
        if (lock) SDL_UnlockSurface(source);
        if (converted != 0) return false;

        if (!valid_ || current_ != previous_) {
            output_.resize(static_cast<size_t>(width * height));
            // This is an opaque, already-composited frame. RGB avoids treating
            // the frame boundary as a transparent sprite edge.
#ifdef __EMSCRIPTEN__
            const unsigned workers = 1;
#else
            const unsigned workers = current_.size() >= 65536 ?
                std::clamp(std::thread::hardware_concurrency(), 1u, 4u) : 1u;
#endif
            // xBRZ accepts independent, non-overlapping output row slices and
            // reads the neighboring source rows itself. SDL stays on this thread.
            std::array<std::future<void>, 3> jobs;
            for (unsigned i = 1; i < workers; ++i) {
                jobs[i - 1] = std::async(std::launch::async, [&, i, workers] {
                    xbrz::scale(factor, current_.data(), output_.data(), source->w, source->h,
                        xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), source->h * i / workers, source->h * (i + 1) / workers);
                });
            }
            xbrz::scale(factor, current_.data(), output_.data(), source->w, source->h,
                xbrz::ColorFormat::RGB, xbrz::ScalerCfg(), 0, source->h / workers);
            for (unsigned i = 1; i < workers; ++i) jobs[i - 1].get();
            if (SDL_UpdateTexture(texture_, nullptr, output_.data(), static_cast<int>(width * sizeof(uint32_t))) != 0)
                return false;
            previous_.swap(current_);
            valid_ = true;
            ++filteredFrames_;
        }
    } catch (const std::exception&) {
        valid_ = false;
        return false;
    }
    SDL_SetTextureScaleMode(texture_, destination.w == width && destination.h == height ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
    return SDL_RenderCopy(renderer, texture_, nullptr, &destination) == 0;
}
