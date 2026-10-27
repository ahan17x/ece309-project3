// reference/p2/src/sentinel_scanner.cpp — INSTRUCTOR REFERENCE for P2.

#include "core/sentinel_scanner.h"

#include <algorithm>

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (sentinel_.empty()) return {std::string(chunk), false};

    std::string combined;
    combined.reserve(pending_.size() + chunk.size());
    combined += pending_;
    combined.append(chunk.data(), chunk.size());

    const std::size_t pos = combined.find(sentinel_);
    if (pos != std::string::npos) {
        pending_.clear();
        return {combined.substr(0, pos), true};
    }

    // Hold back the longest suffix of `combined` that is a proper prefix of
    // the sentinel. It is at most sentinel_.size() - 1 characters long.
    const std::size_t max_hold = std::min(combined.size(), sentinel_.size() - 1);
    for (std::size_t len = max_hold; len > 0; --len) {
        if (combined.compare(combined.size() - len, len, sentinel_, 0, len) == 0) {
            pending_ = combined.substr(combined.size() - len);
            combined.resize(combined.size() - len);
            return {std::move(combined), false};
        }
    }
    pending_.clear();
    return {std::move(combined), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{std::move(pending_), false};
    pending_.clear();
    return out;
}
