// src/tools/file_tools.cpp — PROVIDED.
//
// read_file, write_file, list_dir. Every path goes through
// ctx.workspace.resolve() (yours) and then ctx.workspace.check_symlinks()
// (provided) before any file is touched.

#include "tools/file_tools.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Small helpers for building schemas without a JSON literal.
JsonValue prop(const char* type, const char* description) {
    JsonValue p = JsonValue::object();
    p.set("type", JsonValue::string(type));
    p.set("description", JsonValue::string(description));
    return p;
}

JsonValue make_schema(std::vector<std::pair<std::string, JsonValue>> props,
                      std::vector<std::string> required) {
    JsonValue s = JsonValue::object();
    s.set("type", JsonValue::string("object"));
    JsonValue p = JsonValue::object();
    for (auto& kv : props) p.set(kv.first, std::move(kv.second));
    s.set("properties", std::move(p));
    JsonValue r = JsonValue::array();
    for (auto& name : required) r.push_back(JsonValue::string(name));
    s.set("required", std::move(r));
    return s;
}

// Fetches a required string argument, or explains what is wrong.
bool get_string(const JsonValue& args, const char* key, std::string* out, std::string* err) {
    const JsonValue* v = args.find(key);
    if (v == nullptr) { *err = std::string("missing required argument: ") + key; return false; }
    if (v->type() != JsonValue::Type::String) {
        *err = std::string("argument '") + key + "' must be a string";
        return false;
    }
    *out = v->as_string();
    return true;
}

// Optional bool; absent -> default. Wrong type -> error.
bool get_bool(const JsonValue& args, const char* key, bool dflt, bool* out, std::string* err) {
    const JsonValue* v = args.find(key);
    if (v == nullptr) { *out = dflt; return true; }
    if (v->type() != JsonValue::Type::Bool) {
        *err = std::string("argument '") + key + "' must be a boolean";
        return false;
    }
    *out = v->as_bool();
    return true;
}

}  // namespace

// ---------------------------------------------------------------- read_file

ReadFileTool::ReadFileTool()
    : schema_(make_schema(
          {{"path", prop("string", "File path relative to the workspace root")},
           {"max_bytes", prop("number", "Maximum number of bytes to return")}},
          {"path"})) {}

std::string ReadFileTool::description() const {
    return "Read a text file inside the workspace and return its contents.";
}

ToolResult ReadFileTool::invoke(const JsonValue& args, ToolContext& ctx) {
    std::string path, err;
    if (!get_string(args, "path", &path, &err)) return ToolResult::failure(err);

    std::size_t limit = ctx.max_read_bytes;
    if (const JsonValue* mb = args.find("max_bytes")) {
        if (mb->type() != JsonValue::Type::Number || mb->as_number() < 0 ||
            mb->as_number() != std::floor(mb->as_number()))
            return ToolResult::failure("argument 'max_bytes' must be a non-negative integer");
        if (mb->as_number() < static_cast<double>(limit))
            limit = static_cast<std::size_t>(mb->as_number());
    }

    fs::path resolved;
    std::string reason;
    if (!ctx.workspace.resolve(path, &resolved, &reason)) return ToolResult::failure(reason);
    if (!ctx.workspace.check_symlinks(resolved, &reason)) return ToolResult::failure(reason);

    std::error_code ec;
    auto st = fs::status(resolved, ec);
    if (ec || !fs::exists(st)) return ToolResult::failure("no such file: " + path);
    if (fs::is_directory(st)) return ToolResult::failure("is a directory: " + path);
    if (!fs::is_regular_file(st)) return ToolResult::failure("not a regular file: " + path);

    std::ifstream in(resolved, std::ios::binary);
    if (!in) return ToolResult::failure("cannot open: " + path);

    std::string data(limit, '\0');
    in.read(&data[0], static_cast<std::streamsize>(limit));
    data.resize(static_cast<std::size_t>(in.gcount()));

    const auto total = fs::file_size(resolved, ec);
    if (!ec && total > data.size())
        data += "\n[truncated: showing " + std::to_string(data.size()) + " of " +
                std::to_string(total) + " bytes]";
    return ToolResult::success(std::move(data));
}

// --------------------------------------------------------------- write_file

WriteFileTool::WriteFileTool()
    : schema_(make_schema(
          {{"path", prop("string", "File path relative to the workspace root")},
           {"content", prop("string", "Text to write")},
           {"append", prop("boolean", "Append instead of overwriting (default false)")}},
          {"path", "content"})) {}

std::string WriteFileTool::description() const {
    return "Write (or append) text to a file inside the workspace, creating parent directories.";
}

ToolResult WriteFileTool::invoke(const JsonValue& args, ToolContext& ctx) {
    std::string path, content, err;
    bool append = false;
    if (!get_string(args, "path", &path, &err)) return ToolResult::failure(err);
    if (!get_string(args, "content", &content, &err)) return ToolResult::failure(err);
    if (!get_bool(args, "append", false, &append, &err)) return ToolResult::failure(err);

    fs::path resolved;
    std::string reason;
    if (!ctx.workspace.resolve(path, &resolved, &reason)) return ToolResult::failure(reason);
    if (!ctx.workspace.check_symlinks(resolved, &reason)) return ToolResult::failure(reason);
    if (resolved == ctx.workspace.root())
        return ToolResult::failure("cannot write to the workspace root itself");

    std::error_code ec;
    if (fs::is_directory(resolved, ec)) return ToolResult::failure("is a directory: " + path);

    fs::create_directories(resolved.parent_path(), ec);
    if (ec) return ToolResult::failure("cannot create parent directories: " + ec.message());

    std::ofstream out(resolved, std::ios::binary | (append ? std::ios::app : std::ios::trunc));
    if (!out) return ToolResult::failure("cannot open for writing: " + path);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) return ToolResult::failure("write failed: " + path);

    return ToolResult::success("wrote " + std::to_string(content.size()) + " bytes to " + path);
}

// ----------------------------------------------------------------- list_dir

ListDirTool::ListDirTool()
    : schema_(make_schema(
          {{"path", prop("string", "Directory relative to the workspace root (default \".\")")},
           {"recursive", prop("boolean", "Descend into subdirectories (default false)")}},
          {})) {}

std::string ListDirTool::description() const {
    return "List a directory inside the workspace: sorted entries with file sizes.";
}

ToolResult ListDirTool::invoke(const JsonValue& args, ToolContext& ctx) {
    std::string path = ".", err;
    bool recursive = false;
    if (args.find("path") != nullptr && !get_string(args, "path", &path, &err))
        return ToolResult::failure(err);
    if (!get_bool(args, "recursive", false, &recursive, &err)) return ToolResult::failure(err);

    fs::path dir;
    std::string reason;
    if (!ctx.workspace.resolve(path, &dir, &reason)) return ToolResult::failure(reason);
    if (!ctx.workspace.check_symlinks(dir, &reason)) return ToolResult::failure(reason);

    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return ToolResult::failure("not a directory: " + path);

    std::vector<std::string> lines;
    auto describe = [&](const fs::directory_entry& e) {
        std::string rel = e.path().lexically_relative(dir).generic_string();
        std::error_code sec;
        auto st = e.symlink_status(sec);
        if (fs::is_symlink(st))            lines.push_back(rel + "@");
        else if (fs::is_directory(st))     lines.push_back(rel + "/");
        else if (fs::is_regular_file(st))  lines.push_back(rel + "\t" + std::to_string(e.file_size(sec)));
        else                               lines.push_back(rel + "?");
    };

    if (recursive) {
        // Default options: directory symlinks are NOT followed.
        for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            describe(*it);
    } else {
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
            describe(*it);
    }
    if (ec) return ToolResult::failure("cannot list directory: " + ec.message());

    std::sort(lines.begin(), lines.end());
    if (lines.empty()) return ToolResult::success("(empty directory)");
    std::string outtext;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) outtext += '\n';
        outtext += lines[i];
    }
    return ToolResult::success(std::move(outtext));
}
