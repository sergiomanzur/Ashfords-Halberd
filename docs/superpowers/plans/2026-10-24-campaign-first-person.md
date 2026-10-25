# Campaign first-person implementation plan

**Goal:** Optional eye-level campaign camera with keyboard/mouse and controller support, preserving campaign scripts and native Battle Game first person.

**Architecture:** Extend the existing PC free-camera layer with mode 2 (0 original, 1 third person). Reuse its aim, collision, view-cut restoration and input ownership. Investigate Battle Game eye placement/model rules, but do not set Battle-only mode flags in campaign. Keep scripted sequences and native weapon scopes on their original cameras. Hide the local player body only while drawing active first person; keep pose/collision and other actors intact.

**Builder choices:** First person shares free-camera FOV/sensitivity and bindings; dedicated F12/L3+R3 toggle is rebindable. Eye movement stays stable; unsafe stairs/pushing/script animations temporarily use the original view. No new retail assets.

- [x] Trace Battle first-person setup and rendering.
- [x] Add failing tests for mode selection, camera placement, body visibility and fallback.
- [x] Implement eye-level camera, input behavior and draw-only local-body hiding.
- [x] Add both-menu configuration, persisted mode and keyboard/controller toggle.
- [x] Extend snapshot validation for mode 2; review safety and preserve original scopes/Battle.
- [x] Build and run targeted/full tests, real campaign smoke with look/movement/aim/menu/fallback.
- [x] Update README and package the tested executable.

**Verification limits:** Opening fixtures cannot establish a complete first-person campaign playthrough; late-game interactions need manual playtesting.
