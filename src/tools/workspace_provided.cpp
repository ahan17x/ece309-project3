// src/tools/workspace_provided.cpp — PROVIDED.
//
// The parts of Workspace that need operating-system knowledge (symbolic
// links). You implement Workspace::resolve in src/tools/workspace.cpp.

#include "tools/workspace.h"

#include <system_error>
#include <utility>

namespace fs = std::filesystem;

Workspace::Workspace(fs::path root) {
    std::error_code ec;
    fs::path canon = fs::canonical(root, ec);   // resolves symlinks in the root itself
    root_ = ec ? fs::absolute(root).lexically_normal() : std::move(canon);
}

const fs::path& Workspace::root() const noexcept { return root_; }

// True if `p` is `base` or lies below it, comparing whole path components
// (so "/work2" is NOT inside "/work").
static bool is_within(const fs::path& base, const fs::path& p) {
    auto q = p.begin();
    for (auto b = base.begin(); b != base.end(); ++b) {
        if (b->empty()) continue;   // trailing-separator artifact
        if (q == p.end() || *b != *q) return false;
        ++q;
    }
    return true;
}

bool Workspace::check_symlinks(const fs::path& resolved, std::string* reason) const {
    // 1. Follow every symlink in the part of the path that exists, then make
    //    sure the real location is still inside root.
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(resolved, ec);
    if (ec) {
        *reason = "cannot resolve path: " + ec.message();
        return false;
    }
    if (!is_within(root_, canon)) {
        *reason = "path escapes the workspace through a symbolic link";
        return false;
    }
    // 2. weakly_canonical stops at the first component that doesn't exist,
    //    and a symlink whose target is missing counts as "doesn't exist".
    //    Any symlink still present below root is therefore dangling, and
    //    writing through it could create a file outside the workspace.
    fs::path walk = root_;
    auto it = canon.begin();
    for (auto r = root_.begin(); r != root_.end(); ++r)
        if (!r->empty() && it != canon.end()) ++it;
    for (; it != canon.end(); ++it) {
        if (it->empty()) continue;
        walk /= *it;
        if (fs::is_symlink(fs::symlink_status(walk, ec))) {
            *reason = "path goes through a dangling symbolic link";
            return false;
        }
    }
    return true;
}
