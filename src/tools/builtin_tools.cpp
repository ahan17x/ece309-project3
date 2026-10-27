// src/tools/builtin_tools.cpp — PROVIDED.

#include "tools/builtin_tools.h"
#include "tools/calculator_tool.h"
#include "tools/file_tools.h"

#include <algorithm>
#include <memory>
#include <vector>

std::size_t register_builtin_tools(ToolRegistry& registry) {
    std::size_t added = 0;
    added += registry.add(std::make_unique<ReadFileTool>());
    added += registry.add(std::make_unique<WriteFileTool>());
    added += registry.add(std::make_unique<ListDirTool>());
    added += registry.add(std::make_unique<CalculatorTool>());
    // To add a tool: write a new Tool subclass, then add one line here.
    return added;
}

std::string describe_tools(const ToolRegistry& registry) {
    std::vector<const Tool*> tools;
    registry.for_each([&](const Tool& t) { tools.push_back(&t); });
    std::sort(tools.begin(), tools.end(),
              [](const Tool* a, const Tool* b) { return a->name() < b->name(); });

    std::string out;
    for (const Tool* t : tools) {
        out += "- " + t->name() + ": " + t->description() + "\n";
        out += "  parameters: " + t->schema().dump() + "\n";
    }
    return out;
}
