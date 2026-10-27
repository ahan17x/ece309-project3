// src/harness/transcript.cpp — PROVIDED.

#include "harness/transcript.h"

#include <fstream>

bool save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    for (const Message& m : conv) {
        file << "role: " << role_to_string(m.role()) << "\n"
             << m.content() << "\n"
             << "---\n";
    }
    return static_cast<bool>(file);
}

bool load_transcript(const std::string& path, Conversation* out, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        *error = "cannot open " + path;
        return false;
    }

    std::string line, content;
    bool in_block = false, have_content = false;
    Role role = Role::System;
    int line_no = 0;

    auto finish = [&]() {
        if (in_block) out->append(Message(role, content));
        in_block = false;
        have_content = false;
        content.clear();
    };

    while (std::getline(file, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "---") { finish(); continue; }
        if (!in_block) {
            if (line.empty()) continue;   // tolerate blank lines between blocks
            const std::string prefix = "role: ";
            if (line.compare(0, prefix.size(), prefix) != 0 ||
                !role_from_string(line.substr(prefix.size()), &role)) {
                *error = path + ":" + std::to_string(line_no) + ": expected 'role: <role>'";
                return false;
            }
            in_block = true;
            continue;
        }
        if (have_content) content += '\n';
        content += line;
        have_content = true;
    }
    finish();
    return true;
}
