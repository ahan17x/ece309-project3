# Put your P2 files here

Copy these four files from your Project 2 repository:

| From your P2 repo | To |
|---|---|
| `include/core/conversation.h`     | `include/core/conversation.h` |
| `include/core/sentinel_scanner.h` | `include/core/sentinel_scanner.h` |
| `src/conversation.cpp`            | `src/core/conversation.cpp` |
| `src/sentinel_scanner.cpp`        | `src/core/sentinel_scanner.cpp` |

Do **not** copy `message.h`. P3 ships a new one that adds `Role::Tool`.

If your P2 code doesn't work, configure with `cmake -S . -B build -DUSE_P2_REFERENCE=ON`
to build against the instructor's reference in `reference/p2/` instead.
Say which one you used in your design log. To switch back to your own code
later, reconfigure with `-DUSE_P2_REFERENCE=OFF` (CMake remembers the setting).
