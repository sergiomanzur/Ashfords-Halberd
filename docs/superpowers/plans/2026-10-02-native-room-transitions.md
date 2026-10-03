# Native room transitions implementation plan

Goal: advance the approved Step 1 playable slice by consuming original door
requests and entering the authored destination. Complete gameplay, audio,
inventory, combat and save/load remain required by the parent scope.

1. Port the represented state changes of `room.c::bhSetDoorDemo` into scenario
   opcode 51. Add failing state-transition assertions in scenario-tests first;
   verify subsystem freeze, player state, control flags and menu-exit flags.
2. Add a transactional door-entry core in runtime.hpp/runtime.cpp. Require a
   pending door and validate the destination data before replacing the runtime.
   Preserve persistent progress/inventory, clear completed door control, and
   use the original authored position. Test failed loads retain the old room.
3. Resolve destination AFS entries through the pinned `rdx_files` ordering,
   including room cases. Investigate and implement the required destination
   initialization/controllers from source; never silence unsupported commands.
4. Connect the application preview only after both-room entry and control are
   verified. Map examine input, consume requests, and add a headless transition
   diagnostic. Original door animation/audio remain required follow-up work.
5. Run relevant synthetic suites, retail corridor-to-graveyard and return
   checks, full headless build/tests, and packaged runtime smoke. Record actual
   behavior and remaining limits in status documentation, then commit.
