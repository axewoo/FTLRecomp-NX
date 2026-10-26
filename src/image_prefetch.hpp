#pragma once
#include "archive.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Background PNG decoding. This has no original-game counterpart: the original
// decodes artwork as it is first drawn, which on Switch stalls a frame for
// tens of milliseconds. A worker decodes the commonly used artwork ahead of
// time; the render thread only uploads finished images.
namespace ftl {

struct DecodedImage {
    std::string key; // archiveKey form of the archive path
    Bytes pixels;    // RGBA, empty when decoding failed
    int w = 0;
    int h = 0;
    std::string failure;
};

// Reads and decodes one archive PNG. Safe to call from any thread.
DecodedImage decodeImage(const Archive& archive, const std::string& path);

class ImagePrefetcher {
public:
    // Decodes keys (archiveKey form) in order on a worker thread.
    ImagePrefetcher(const Archive& archive, std::vector<std::string> keys);
    ~ImagePrefetcher();
    ImagePrefetcher(const ImagePrefetcher&) = delete;
    ImagePrefetcher& operator=(const ImagePrefetcher&) = delete;

    // The decoded image for key: waits while the worker is decoding it, and
    // drops it from the queue when the worker has not reached it yet (the
    // caller then decodes it itself). Render thread only.
    std::optional<DecodedImage> take(const std::string& key);
    // One finished image, if any. Render thread only.
    std::optional<DecodedImage> next();
    // True when every queued image has been decoded and handed out.
    bool finished();

private:
    void run();

    const Archive& archive_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::deque<std::string> pending_;
    std::deque<DecodedImage> ready_;
    std::string current_;
    bool stop_ = false;
    struct Worker;
    std::unique_ptr<Worker> worker_;
};
} // namespace ftl
