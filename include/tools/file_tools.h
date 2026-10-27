// include/tools/file_tools.h
//
// PROVIDED. The three file tools. Each one calls ctx.workspace.resolve()
// (your code) and then ctx.workspace.check_symlinks() (provided) before
// touching the filesystem.

#pragma once
#include "tools/tool.h"

// read_file  {"path": string, "max_bytes"?: number}
//   Returns the file's bytes, truncated to min(max_bytes, ctx.max_read_bytes).
class ReadFileTool : public Tool {
public:
    ReadFileTool();
    std::string      name() const override { return "read_file"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};

// write_file {"path": string, "content": string, "append"?: bool}
//   Creates parent directories as needed. Returns "wrote N bytes to PATH".
class WriteFileTool : public Tool {
public:
    WriteFileTool();
    std::string      name() const override { return "write_file"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};

// list_dir   {"path": string, "recursive"?: bool}
//   One entry per line, sorted by path: "name/" for directories,
//   "name<TAB>size" for regular files, "name@" for symlinks (never followed).
class ListDirTool : public Tool {
public:
    ListDirTool();
    std::string      name() const override { return "list_dir"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};
