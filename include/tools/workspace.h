// include/tools/workspace.h
//
// The folder the file tools are allowed to touch.
//   - constructor, root(), check_symlinks(): PROVIDED (src/tools/workspace_provided.cpp)
//   - resolve():                             YOU IMPLEMENT (src/tools/workspace.cpp)
//
// Every file tool does, in this order:
//   ctx.workspace.resolve(user_path, &real, &why)     // text check    (yours)
//   ctx.workspace.check_symlinks(real, &why)          // symlink check (provided)
// and only then opens `real`.

#pragma once
#include <filesystem>
#include <string>
#include <string_view>

class Workspace {
public:
    // Provided. Stores the absolute, symlink-free form of `root`.
    explicit Workspace(std::filesystem::path root);

    // Provided. The root chosen by the constructor.
    const std::filesystem::path& root() const noexcept;

    // YOU IMPLEMENT (spec §4.4). Maps a model-supplied, workspace-relative
    // path to an absolute path inside root(). The target need not exist yet.
    //   success:   returns true and stores the path in *out
    //   rejection: returns false and stores a non-empty explanation in *reason
    // Must not throw and must not touch the filesystem. `out` and `reason`
    // are never null.
    bool resolve(std::string_view user_path,
                 std::filesystem::path* out,
                 std::string* reason) const;

    // Provided. Call this *after* resolve() succeeds, before any file I/O.
    // Rejects paths that escape through symbolic links (including links whose
    // target does not exist yet). Returns false and sets *reason on rejection.
    bool check_symlinks(const std::filesystem::path& resolved,
                        std::string* reason) const;

private:
    std::filesystem::path root_;   // absolute; set by the constructor
};
