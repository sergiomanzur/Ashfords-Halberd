# Native room loading

Continue approved Step 1 with a headless, portable asset reader. The Windows executable remains a foundation menu until gameplay and rendering work is ready.

- Write and run failing tests for AFS extents, the game's Expand codec, and room metadata bounds.
- Implement bounded readers using explicit little-endian values; never reinterpret PS2 pointers as host pointers.
- Add a command-line inspector which reads the user's cached RDX_LNK.AFS, decodes each room, and reports camera, collision, spawn, and model metadata.
- Validate every retail room without opening windows. Run the existing headless suite again.

The decoder follows the documented Expand algorithm in upstream expand.c (including Clownacy's explanatory C version). No original assets enter source control or the distribution.

Implemented: src/assets.hpp/.cpp, tests/asset_tests.cpp, and tools/room_inspect.cpp.
The initial test compile failed because the asset API did not exist; the bounded
implementation then passed. All 205 retail room entries validated successfully.
The inspector remains a development tool; the shipping menu does not claim
room rendering or gameplay.
