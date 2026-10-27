// include/tools/builtin_tools.h
//
// PROVIDED. The tool "factory" and the --tools listing.

#pragma once
#include "tools/tool_registry.h"

#include <string>

// Registers read_file, write_file, list_dir, and calculator. Adding a new
// tool to the harness means writing a new Tool subclass and adding ONE line
// here; no existing tool's code changes.
// Returns the number of tools successfully added.
std::size_t register_builtin_tools(ToolRegistry& registry);

// Human-readable listing used by `--tools` and injected into the system
// message. Tools are sorted by name. Format, per tool:
//   - NAME: DESCRIPTION
//     parameters: SCHEMA_JSON
std::string describe_tools(const ToolRegistry& registry);
