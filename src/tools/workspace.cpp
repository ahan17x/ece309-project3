#include "tools/workspace.h"

#include <string>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out, std::string* reason) const {
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }

    fs::path p{std::string(user_path)};
    if (p.is_absolute() || p.has_root_path()) {
        *reason = "absolute paths are not allowed; use a path relative to the workspace";
        return false;
    }

    fs::path n = p.lexically_normal();
    if (!n.empty() && *n.begin() == "..") {
        *reason = "path escapes the workspace: " + std::string(user_path);
        return false;
    }

    if (n.empty() || n == ".") {
        *out = root();
    } else {
        *out = root() / n;
    }
    return true;
}
