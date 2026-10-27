// src/main.cpp — PROVIDED.
//
// Usage:
//   miniharness [--script FILE | --replay FILE] [--save FILE]
//               [--max-turns N] [--max-tool-steps N]
//               [--workspace DIR] [--no-tools] [--tools]
//
//   --tools        print every registered tool (name, description, schema)
//                  and exit
//   --no-tools     run exactly like P2: no registry, no system message
//   --workspace    root directory the file tools may touch (default:
//                  ./workspace, created if missing)

#include "harness/harness.h"
#include "harness/transcript.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"
#include "tools/builtin_tools.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

class StdioInput : public InputSource {
public:
    std::string read_line() override {
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        eof_ = std::cin.eof() && line.empty();
        return line;
    }
    bool is_eof() const override { return eof_; }
private:
    bool eof_ = false;
};

class StdioOutput : public OutputSink {
public:
    void write(std::string_view text) override { std::cout << text << std::flush; }
};

std::string build_system_message(const ToolRegistry& registry) {
    return "You are MiniHarness, a helpful assistant.\n"
           "To use a tool, emit <|tool_call|>{\"id\":\"...\",\"name\":\"...\",\"arguments\":{...}}<|/tool_call|>\n"
           "Available tools:\n" +
           describe_tools(registry);
}

int usage(const char* argv0) {
    std::cerr << "usage: " << argv0
              << " [--script FILE | --replay FILE] [--save FILE] [--max-turns N]\n"
                 "       [--max-tool-steps N] [--workspace DIR] [--no-tools] [--tools]\n";
    return 2;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string script_path = "scripts/greeting.script";
    std::string replay_path, save_path, workspace_dir = "workspace";
    bool list_tools = false, no_tools = false;
    HarnessConfig config;

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
                return argv[++i];
            };
            if      (arg == "--script")         script_path = next();
            else if (arg == "--replay")         replay_path = next();
            else if (arg == "--save")           save_path = next();
            else if (arg == "--max-turns")      config.max_turns = std::stoi(next());
            else if (arg == "--max-tool-steps") config.max_tool_steps = std::stoi(next());
            else if (arg == "--workspace")      workspace_dir = next();
            else if (arg == "--tools")          list_tools = true;
            else if (arg == "--no-tools")       no_tools = true;
            else return usage(argv[0]);
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return usage(argv[0]);
    }

    try {
        // With --no-tools nothing from P3 is constructed, so the harness runs
        // even before your P3 classes work.
        std::optional<ToolRegistry> registry;
        std::optional<ToolContext>  ctx;
        if (!no_tools || list_tools) {
            registry.emplace();
            register_builtin_tools(*registry);
        }

        if (list_tools) {
            std::cout << describe_tools(*registry);
            return 0;
        }

        if (!no_tools) {
            std::filesystem::create_directories(workspace_dir);
            ctx.emplace(Workspace(workspace_dir));
        }

        std::unique_ptr<ModelClient> model;
        if (!replay_path.empty()) model = std::make_unique<ReplayModelClient>(replay_path);
        else                      model = std::make_unique<ScriptedModelClient>(script_path);

        std::unique_ptr<Harness> harness;
        if (no_tools) {
            harness = std::make_unique<Harness>(std::move(model), config);
        } else {
            config.system_message = build_system_message(*registry);
            harness = std::make_unique<Harness>(std::move(model), config, &*registry, &*ctx);
        }

        StdioInput in;
        StdioOutput out;
        StopReason reason = harness->run(in, out);
        std::cout << "[conversation ended: " << reason.detail << "]\n";

        if (!save_path.empty()) {
            if (save_transcript(harness->conversation(), save_path))
                std::cout << "[transcript saved to " << save_path << "]\n";
            else
                std::cerr << "error: could not write " << save_path << "\n";
        }
        return reason.kind == StopReason::Kind::ClientError ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
