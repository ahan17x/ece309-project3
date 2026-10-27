// include/harness/tool_dispatcher.h
//
// PROVIDED. Turns <|tool_call|> blocks in an assistant message into
// Role::Tool result messages, using the provided JsonValue parser and
// YOUR ToolRegistry.

#pragma once
#include "core/message.h"
#include "tools/tool_registry.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::string_view kToolCallOpen    = "<|tool_call|>";
inline constexpr std::string_view kToolCallClose   = "<|/tool_call|>";
inline constexpr std::string_view kToolResultOpen  = "<|tool_result|>";
inline constexpr std::string_view kToolResultClose = "<|/tool_result|>";

// One executed (or rejected) call, for display and for tests.
struct DispatchedCall {
    std::string id;         // "" if the call was too malformed to have one
    std::string tool_name;  // "" if unknown / malformed
    ToolResult  result;
};

class ToolDispatcher {
public:
    explicit ToolDispatcher(ToolRegistry& registry, std::size_t max_calls_per_message = 16);

    // True if `text` contains at least one <|tool_call|> opening tag.
    static bool has_tool_call(std::string_view text);

    // Extracts every <|tool_call|>...<|/tool_call|> block from the assistant
    // message, in order, invokes each, and returns one Role::Tool message per
    // block. Malformed blocks (bad JSON, missing name, unknown tool,
    // unterminated tag, non-object arguments) become error results — never
    // crashes, never throws. If `calls_out` is non-null, one DispatchedCall
    // per block is appended to it.
    std::vector<Message> handle(const Message& assistant_msg, ToolContext& ctx,
                                std::vector<DispatchedCall>* calls_out = nullptr);

    // The wire format of one result:
    //   <|tool_result|>{"id":"c1","ok":true,"content":"..."}<|/tool_result|>
    //   <|tool_result|>{"id":"c1","ok":false,"error":"..."}<|/tool_result|>
    static std::string format_result(const std::string& id, const ToolResult& r);

private:
    ToolRegistry& registry_;
    std::size_t   max_calls_;
};
