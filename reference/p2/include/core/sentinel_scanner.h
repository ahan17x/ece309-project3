// reference/p2/include/core/sentinel_scanner.h — INSTRUCTOR REFERENCE for P2.

#pragma once
#include <string>
#include <string_view>
#include <utility>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel) : sentinel_(std::move(sentinel)) {}

    struct Out { std::string safe_text; bool sentinel_found; };

    Out feed(std::string_view chunk);
    Out flush();

    // Exposed so tests can verify the bounded-memory invariant.
    std::size_t pending_size() const noexcept { return pending_.size(); }

private:
    std::string sentinel_;
    std::string pending_;   // never longer than sentinel_.size() - 1
};
