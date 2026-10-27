// include/core/sentinel_scanner.h
// Incrementally detects a multi-character sentinel in a character stream
// that arrives in arbitrary chunks, using O(sentinel length) memory.

#pragma once
#include <string>
#include <string_view>

class SentinelScanner {
public:
    // Throws std::invalid_argument if sentinel is empty.
    explicit SentinelScanner(std::string sentinel);

    struct Out {
        std::string safe_text;   // text guaranteed not to be part of the sentinel
        bool        sentinel_found;
    };

    // Feed the next chunk. Returns text guaranteed NOT to be part of the
    // sentinel (safe to print immediately) and whether the sentinel has
    // now been fully seen. When it is found, everything from the sentinel
    // onward (including any text after it in the same chunk) is dropped.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still being
    // held back.
    Out flush();

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                            // trailing characters that could still
                            // become the start of the sentinel
};
