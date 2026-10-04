# Native original room fog implementation plan

> REQUIRED SUB-SKILL: superpowers:executing-plans; authorized inline execution.

BASE c7d719a. Spec:2026-10-02-windows-port-design.md in this directory's
../specs. Goal remains complete Step1: two connected original rooms, gameplay,
combat/inventory, complete sound/effects, versioned cross-room saves and original
reference acceptance. New Game stays disabled. All tests are headless with dummy
devices and fresh ignored profiles; no desktop, audible output, push or merge.

Port original event76 through actual fog consumers. Source event.c4067 consumes
six bytes76,A,R,G,B,mode. Mode0 stores ARGB and sets state100000/game10; other
modes retain ARGB, clear state100000 and request game10. ROM header fog at
108/10C/110 and camera variants atB8/BC/C0 contain signed floats. Actual corridor
and cemetery near values include positive128: preserve source values. Do not
replace the source128-entry table with a generic linear-distance equation.

Architecture: native pure fog parameters/table/work; current scene snapshots
own active fog; ScenarioState owns original override/countdown and EOR control;
camera cuts select authored values. Rasterization samples per-vertex original
njCalcFogPowerEx, carries fog through clipping and interpolates screen fog before
GS256 color mixing. Object/item mdflg10 suppression follows original draw passes.
Standalone camera viewing uses authored fog; runtime scene state overrides it,
including disabled state. Room/cut binding and control are transactional.

Full dynamic/keyframe/eye camera implementations, original direct lighting,
remaining effects and independent PS2 rendering comparison stay required. A
controller API can represent source eye/keyframe fog selection with explicit
inputs; unsupported camera binding must reject rather than silently choose a cut.

## Task1: source layouts, command and numerical fog

- [x] Compile/run actual76 literal against BASE; behavioral RED unsupported76.
- [x] Build independent native reference from unchanged MIT source functions;
  record whole-table hashes and exact sampled float bits for actual signed
  parameters, quantized density, near/far clamping and special cases. This is
  reference math evidence, not an original gameplay/visual acceptance substitute.
- [x] Write tests before implementation for header/camera signed fields, six-byte
  cursor/flags/mode retention/malformed input, exact table/sample reference,
  EOR dirty/countdown/disable/override/eye/keyframe branches and cut countdown.
- [x] Implement endian decoding without pointer casts; generated table and
  float-bit lookup preserve source operations. Invalid native extents/nonfinite
  inputs reject before publication; source-defined signed values remain intact.

## Task2: real native pixels and runtime consumers

- [x] Write/run source-authored camera/pixel fixture on unchanged BASE rasterizer.
  Behavioral RED: existing texture color survives where original fog must mix.
- [x] Preserve fog through camera resolution, Scene composition, object/item
  suppression and rasterized pixels. Test uniform/varying depth, authored camera
  versus current runtime color/disabled state, clipping, alpha/depth rejection,
  malformed selected table and renderer material staging.
- [x] Bind destination fog before initialization, dirty on entry/cut change and
  execute native EOR at fixed simulation cadence. Entry/transition failures
  retain old fog/state. Existing physical/user profile flow stays untouched.

## Task3: retail, full verification and one fresh review

- [x] Recompile unchanged retail entry probe and inspect actual active fog for
  corridor/cemetery. No script skipping or forced branches. Record next service.
- [x] Full tools/build.ps1, fresh packaged dummy runtime, exact six-file ZIP/hash,
  source formatting and diff checks. Full suite and native package must pass.
- [x] One fresh read-only gpt-6-astra/high review, no child/runtime/build/mutation.
  Focus1 source layouts/76;2 table/sample arithmetic;3 EOR/cut/runtime lifetime;
  4 actual pixel/mesh/clip interpolation;5 real probe/package/scope evidence.
  Re-grade Critical/Important, one author RED-to-GREEN fix pass/full suite;
  defer Minors and ledger every declined judgment. Local commit; goal active.
