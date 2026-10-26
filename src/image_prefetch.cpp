#include "image_prefetch.hpp"
#include <algorithm>
#include <stb_image.h>
#include <stdexcept>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace ftl {
namespace {
// Decoded images waiting for upload; bounds the memory the worker runs ahead.
constexpr std::size_t readyLimit = 6;
} // namespace

DecodedImage decodeImage(const Archive& archive, const std::string& path) {
    DecodedImage image;
    image.key = archiveKey(path);
    Bytes bytes;
    try {
        bytes = archive.read(path);
    } catch (const std::exception& e) {
        image.failure = e.what();
        return image;
    }
    int channels;
    auto* pixels = stbi_load_from_memory(
        bytes.data(), int(bytes.size()), &image.w, &image.h, &channels, 4);
    if (!pixels) {
        const auto* reason = stbi_failure_reason();
        image.failure = reason ? std::string(reason) : std::string("invalid PNG");
        return image;
    }
    image.pixels.assign(pixels, pixels + std::size_t(image.w) * image.h * 4);
    stbi_image_free(pixels);
    return image;
}

#ifdef __SWITCH__
// The main thread runs on core 0. A std::thread would start there too, and
// Horizon does not time-slice threads of one priority, so use core 1 at a
// slightly lower priority, falling back to the default core.
struct ImagePrefetcher::Worker {
    Thread thread{};
    explicit Worker(ImagePrefetcher* owner) {
        auto entry = [](void* argument) { static_cast<ImagePrefetcher*>(argument)->run(); };
        if (R_FAILED(threadCreate(&thread, entry, owner, nullptr, 0x40000, 0x2D, 1)) &&
            R_FAILED(threadCreate(&thread, entry, owner, nullptr, 0x40000, 0x2D, -2))) {
            throw std::runtime_error("prefetch_thread");
        }
        threadStart(&thread);
    }
    ~Worker() {
        threadWaitForExit(&thread);
        threadClose(&thread);
    }
};
#else
struct ImagePrefetcher::Worker {
    std::thread thread;
    explicit Worker(ImagePrefetcher* owner) : thread([owner] { owner->run(); }) {}
    ~Worker() {
        thread.join();
    }
};
#endif

ImagePrefetcher::ImagePrefetcher(const Archive& archive, std::vector<std::string> keys)
    : archive_(archive), pending_(keys.begin(), keys.end()) {
    worker_ = std::make_unique<Worker>(this);
}

ImagePrefetcher::~ImagePrefetcher() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    changed_.notify_all();
    worker_.reset();
}

void ImagePrefetcher::run() {
    std::unique_lock lock(mutex_);
    while (true) {
        changed_.wait(lock, [&] {
            return stop_ || (!pending_.empty() && ready_.size() < readyLimit);
        });
        if (stop_) {
            return;
        }
        current_ = std::move(pending_.front());
        pending_.pop_front();
        const auto key = current_;
        lock.unlock();
        auto image = decodeImage(archive_, key);
        lock.lock();
        ready_.push_back(std::move(image));
        current_.clear();
        changed_.notify_all();
    }
}

std::optional<DecodedImage> ImagePrefetcher::take(const std::string& key) {
    std::unique_lock lock(mutex_);
    changed_.wait(lock, [&] { return current_ != key; });
    const auto ready = std::ranges::find(ready_, key, &DecodedImage::key);
    if (ready != ready_.end()) {
        auto image = std::move(*ready);
        ready_.erase(ready);
        changed_.notify_all();
        return image;
    }
    if (const auto queued = std::ranges::find(pending_, key); queued != pending_.end()) {
        pending_.erase(queued);
    }
    return std::nullopt;
}

bool ImagePrefetcher::finished() {
    std::lock_guard lock(mutex_);
    return pending_.empty() && current_.empty() && ready_.empty();
}

std::optional<DecodedImage> ImagePrefetcher::next() {
    std::lock_guard lock(mutex_);
    if (ready_.empty()) {
        return std::nullopt;
    }
    auto image = std::move(ready_.front());
    ready_.pop_front();
    changed_.notify_all();
    return image;
}
} // namespace ftl
