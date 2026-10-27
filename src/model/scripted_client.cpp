// src/model/scripted_client.cpp — PROVIDED.
//
// P3 changes from P2: strips a trailing '\r' from each line (scripts edited
// on Windows), and an exhausted script now throws, which the Harness reports
// as StopReason::Kind::ClientError (as the P2 spec always said it should).

#include "model/scripted_client.h"
#include <fstream>
#include <stdexcept>

// Trims leading whitespace (useful for parsing "chunk: 5")
static std::string trim_left(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start);
}

ScriptedModelClient::ScriptedModelClient(const std::string& script_path) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script file: " + script_path);
    }

    std::string line;
    int current_chunk = -1;
    bool in_assistant_block = false;
    std::string current_content;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "---") {
            // End of a block. If it was an assistant block, save it.
            if (in_assistant_block) {
                blocks_.push_back({current_content, current_chunk});
                current_content.clear();
                in_assistant_block = false;
            }
            current_chunk = -1; // Reset chunk size for the next block
            continue;
        }

        if (!in_assistant_block) {
            // Looking for directives or role headers
            if (line.rfind("chunk:", 0) == 0) {
                try {
                    current_chunk = std::stoi(trim_left(line.substr(6)));
                } catch (...) {
                    current_chunk = -1;
                }
            } else if (line.rfind("match:", 0) == 0) {
                // Baseline implementation ignores regex branching
                continue; 
            } else if (line == "role: assistant") {
                in_assistant_block = true;
            }
            // role: system / user / tool blocks are ignored until the next ---
        } else {
            // We are actively inside an assistant block, accumulate the text
            if (!current_content.empty()) {
                current_content += "\n";
            }
            current_content += line;
        }
    }

    // Handle edge case: EOF reached without a trailing "---"
    if (in_assistant_block) {
        blocks_.push_back({current_content, current_chunk});
    }
}

void ScriptedModelClient::generate(const Conversation& /*conv*/, TokenSink& sink) {
    // Running past the end of the script is a client error, not a reply.
    if (current_block_idx_ >= blocks_.size()) {
        throw std::runtime_error("script exhausted: no more assistant blocks");
    }

    const ScriptBlock& block = blocks_[current_block_idx_++];
    
    // Perform the chunked streaming
    if (block.chunk_size > 0) {
        for (std::size_t i = 0; i < block.content.size(); i += block.chunk_size) {
            sink.on_chunk(block.content.substr(i, block.chunk_size));
        }
    } else {
        // No chunk directive, stream it all at once
        sink.on_chunk(block.content);
    }
    
    sink.on_complete();
}