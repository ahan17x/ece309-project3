// src/harness/tool_dispatcher.cpp — PROVIDED.

#include "harness/tool_dispatcher.h"

#include <exception>
#include <utility>

ToolDispatcher::ToolDispatcher(ToolRegistry& registry, std::size_t max_calls_per_message)
    : registry_(registry), max_calls_(max_calls_per_message) {}

bool ToolDispatcher::has_tool_call(std::string_view text) {
    return text.find(kToolCallOpen) != std::string_view::npos;
}

std::string ToolDispatcher::format_result(const std::string& id, const ToolResult& r) {
    JsonValue obj = JsonValue::object();
    obj.set("id", JsonValue::string(id));
    obj.set("ok", JsonValue::boolean(r.ok));
    if (r.ok) obj.set("content", JsonValue::string(r.content));
    else      obj.set("error", JsonValue::string(r.error));
    return std::string(kToolResultOpen) + obj.dump() + std::string(kToolResultClose);
}

namespace {

// Parses one call body and runs it. Fills `call` in all cases.
void run_one(std::string_view body, ToolRegistry& registry, ToolContext& ctx,
             DispatchedCall* call) {
    std::string err;
    JsonValue v = JsonValue::parse(body, &err);
    if (!err.empty()) {
        call->result = ToolResult::failure("malformed tool call: " + err);
        return;
    }
    if (v.type() != JsonValue::Type::Object) {
        call->result = ToolResult::failure("malformed tool call: expected a JSON object");
        return;
    }
    if (const JsonValue* id = v.find("id"); id && id->type() == JsonValue::Type::String)
        call->id = id->as_string();

    const JsonValue* name = v.find("name");
    if (name == nullptr || name->type() != JsonValue::Type::String) {
        call->result = ToolResult::failure("malformed tool call: missing string field \"name\"");
        return;
    }

    JsonValue empty_args = JsonValue::object();
    const JsonValue* args = v.find("arguments");
    if (args == nullptr) {
        args = &empty_args;
    } else if (args->type() != JsonValue::Type::Object) {
        call->result = ToolResult::failure("malformed tool call: \"arguments\" must be an object");
        return;
    }

    Tool* tool = registry.find(name->as_string());
    if (tool == nullptr) {
        call->result = ToolResult::failure("unknown tool: " + name->as_string());
        return;
    }
    call->tool_name = tool->name();

    // Tools report failure as data; an exception escaping one is a bug in
    // that tool. Contain it so one bad tool cannot take down the harness.
    try {
        call->result = tool->invoke(*args, ctx);
    } catch (const std::exception& e) {
        call->result = ToolResult::failure(std::string("tool raised an exception: ") + e.what());
    } catch (...) {
        call->result = ToolResult::failure("tool raised an unknown exception");
    }
}

}  // namespace

std::vector<Message> ToolDispatcher::handle(const Message& assistant_msg, ToolContext& ctx,
                                            std::vector<DispatchedCall>* calls_out) {
    std::vector<Message> results;
    std::string_view text = assistant_msg.content();
    std::size_t pos = 0;

    while (true) {
        std::size_t open = text.find(kToolCallOpen, pos);
        if (open == std::string_view::npos) break;

        DispatchedCall call;
        std::size_t body_start = open + kToolCallOpen.size();
        std::size_t close = text.find(kToolCallClose, body_start);

        if (results.size() >= max_calls_) {
            call.result = ToolResult::failure("too many tool calls in one message (limit " +
                                              std::to_string(max_calls_) + ")");
            results.emplace_back(Role::Tool, format_result(call.id, call.result));
            if (calls_out) calls_out->push_back(std::move(call));
            break;
        }

        if (close == std::string_view::npos) {
            call.result = ToolResult::failure("malformed tool call: missing <|/tool_call|>");
            results.emplace_back(Role::Tool, format_result(call.id, call.result));
            if (calls_out) calls_out->push_back(std::move(call));
            break;
        }

        run_one(text.substr(body_start, close - body_start), registry_, ctx, &call);
        results.emplace_back(Role::Tool, format_result(call.id, call.result));
        if (calls_out) calls_out->push_back(std::move(call));
        pos = close + kToolCallClose.size();
    }
    return results;
}
