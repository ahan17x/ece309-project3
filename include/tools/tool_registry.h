// include/tools/tool_registry.h
//
// YOU IMPLEMENT (src/tools/tool_registry.cpp). Spec §4.3 has a function-by-
// function table of the exact behavior.
//
// Maps tool names to Tool objects using a HASH TABLE YOU WRITE YOURSELF.
// std::unordered_map, std::map, std::set, std::unordered_set (and any other
// associative container) are forbidden in this class. std::vector, raw arrays,
// and std::unique_ptr are fine.
//
// The PUBLIC interface below is fixed. The PRIVATE section is yours: choose
// separate chaining or open addressing, choose a hash function and a
// max-load-factor threshold, and defend all three in your design log.

#pragma once
#include "tools/tool.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class ToolRegistry {
public:
    ToolRegistry();     // empty; bucket_count() between 1 and 64
    ~ToolRegistry();

    // The registry owns its tools; copying it would mean copying tools.
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    // Takes ownership of `tool` and indexes it under tool->name().
    // Returns false (and destroys `tool`) if `tool` is null or the name is
    // already registered; the existing tool is left untouched.
    // Amortized O(1).
    bool add(std::unique_ptr<Tool> tool);

    // The tool registered under `name`, or nullptr. Expected O(1).
    // The returned pointer stays valid for the registry's lifetime, even
    // across later add() calls that trigger a resize.
    Tool* find(std::string_view name) const;

    // Number of registered tools.
    std::size_t size() const noexcept;

    // Calls fn once per registered tool. Order is unspecified (callers that
    // need a stable order, like --tools, sort the names themselves).
    void for_each(const std::function<void(const Tool&)>& fn) const;

    // ---- Instrumentation (used by tests and your design log) ----------
    // Current number of buckets (chaining) or slots (open addressing).
    std::size_t bucket_count() const noexcept;

    // size() / bucket_count().
    // Must never exceed 1.0 after add() returns.
    double load_factor() const noexcept;

    // Chaining: length of the longest chain.
    // Open addressing: length of the longest probe sequence needed to find
    // any key currently stored (a key found in its home slot counts as 1).
    std::size_t max_probe_length() const;

    // The string hash your table uses (FNV-1a, djb2, ...). Must be a real
    // string hash that looks at every character: s[0] + s.size() is not.
    static std::size_t hash(std::string_view key) noexcept;

private:
    struct Entry {
        std::string name;
        std::size_t hash;
        std::unique_ptr<Tool> tool;
    };

    static constexpr std::size_t kInitialBuckets = 16;
    static constexpr double kMaxLoad = 0.75;

    const Entry* lookup(std::string_view name, std::size_t h) const;
    void rehash(std::size_t new_count);

    std::vector<std::vector<Entry>> buckets_;
    std::size_t count_ = 0;
};
