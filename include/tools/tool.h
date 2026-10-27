// include/tools/tool.h
//
// PROVIDED. The interface every tool implements, the result type every tool
// returns, and the context every tool receives.

#pragma once
#include "json/json_value.h"
#include "tools/workspace.h"

#include <cstddef>
#include <string>
#include <utility>

// Errors are returned as data, not thrown. A tool failing (file not found,
// division by zero, path rejected) is a NORMAL event in an agent loop: the
// model reads the error and tries something else.
struct ToolResult {
    bool        ok = false;
    std::string content;      // shown to the model when ok
    std::string error;        // shown to the model when !ok

    static ToolResult success(std::string content) {
        ToolResult r;
        r.ok = true;
        r.content = std::move(content);
        return r;
    }
    static ToolResult failure(std::string error) {
        ToolResult r;
        r.ok = false;
        r.error = std::move(error);
        return r;
    }
};

// Everything a tool is allowed to touch is passed in here — no globals.
struct ToolContext {
    explicit ToolContext(Workspace ws) : workspace(std::move(ws)) {}

    Workspace   workspace;
    std::size_t max_read_bytes = 64 * 1024;   // hard cap for read_file
};

class Tool {
public:
    virtual ~Tool() = default;

    // Unique, stable identifier the model uses in "name" of a tool call.
    virtual std::string name() const = 0;

    // One sentence for the model: what the tool does.
    virtual std::string description() const = 0;

    // A JSON object describing the parameters, e.g.
    // {"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}
    virtual const JsonValue& schema() const = 0;

    // `args` is the "arguments" value from the tool call; the dispatcher
    // guarantees it is a JSON Object (possibly empty). A tool must validate
    // its own arguments and report problems via ToolResult::failure().
    virtual ToolResult invoke(const JsonValue& args, ToolContext& ctx) = 0;
};
