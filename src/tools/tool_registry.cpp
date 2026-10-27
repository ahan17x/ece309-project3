#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

const ToolRegistry::Entry* ToolRegistry::lookup(std::string_view name, std::size_t h) const {
    for (const Entry& e : buckets_[h & (buckets_.size() - 1)]) {
        if (e.hash == h && e.name == name) return &e;
    }
    return nullptr;
}

void ToolRegistry::rehash(std::size_t new_count) {
    std::vector<std::vector<Entry>> fresh(new_count);
    for (auto& bucket : buckets_) {
        for (auto& e : bucket) {
            fresh[e.hash & (new_count - 1)].push_back(std::move(e));
        }
    }
    buckets_.swap(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;

    std::string name = tool->name();
    std::size_t h = hash(name);
    if (lookup(name, h) != nullptr) return false;

    if (static_cast<double>(count_ + 1) > kMaxLoad * static_cast<double>(buckets_.size())) {
        rehash(buckets_.size() * 2);
    }

    buckets_[h & (buckets_.size() - 1)].push_back({std::move(name), h, std::move(tool)});
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const Entry* e = lookup(name, hash(name));
    return e ? e->tool.get() : nullptr;
}

std::size_t ToolRegistry::size() const noexcept { return count_; }

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const Entry& e : bucket) fn(*e.tool);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) longest = bucket.size();
    }
    return longest;
}
