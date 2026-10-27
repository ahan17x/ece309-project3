// src/sentinel_scanner.cpp
// Streaming sentinel detection with a bounded hold-back buffer.

#include "core/sentinel_scanner.h"
#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) {
        throw std::invalid_argument("SentinelScanner: sentinel must not be empty");
    }
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    // The text under consideration is everything we held back plus the
    // new chunk. |buf| <= (k - 1) + |chunk|, where k = sentinel_.size().
    std::string buf = std::move(pending_);
    pending_.clear();
    buf.append(chunk.data(), chunk.size());

    // Case 1: a complete sentinel is in buf. Everything before it is safe;
    // everything from it onward (including any trailing text) is dropped.
    const std::size_t pos = buf.find(sentinel_);
    if (pos != std::string::npos) {
        buf.resize(pos);
        return {std::move(buf), true};
    }

    // Case 2: no complete sentinel yet. A future occurrence can only
    // overlap buf if some suffix of buf is a proper prefix of the sentinel.
    // Hold back the LONGEST such suffix (at most k - 1 characters); the
    // rest can never be part of the sentinel, so it is safe to emit.
    const std::size_t k       = sentinel_.size();
    const std::size_t max_len = (buf.size() < k - 1) ? buf.size() : k - 1;
    std::size_t hold = 0;
    for (std::size_t len = max_len; len > 0; --len) {
        if (buf.compare(buf.size() - len, len, sentinel_, 0, len) == 0) {
            hold = len;
            break;
        }
    }

    pending_.assign(buf, buf.size() - hold, hold);
    buf.resize(buf.size() - hold);
    return {std::move(buf), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{std::move(pending_), false};
    pending_.clear();
    return out;
}
