// include/harness/transcript.h
//
// PROVIDED. Reads and writes the transcript format (spec Appendix D):
//
//   role: user
//   hello
//   ---
//   role: assistant
//   Hi!
//   ---
//
// A message's content is every line between its "role:" line and the next
// "---" line, joined with '\n'. Content may therefore not contain a line
// that is exactly "---". Tool results are single-line JSON, so they are safe.

#pragma once
#include "core/conversation.h"

#include <string>

// Returns false if the file cannot be opened for writing.
bool save_transcript(const Conversation& conv, const std::string& path);

// Appends every message in the file to *out. Returns false (with a reason)
// if the file cannot be opened or a block has a missing/unknown role.
bool load_transcript(const std::string& path, Conversation* out, std::string* error);
