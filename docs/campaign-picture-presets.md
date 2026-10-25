# Picture controls and presets — 2026-10-24

Implements the requested [picture controls (#56)](https://github.com/aikatore/recvx-enhanced-port/issues/56)
and [simpler configuration (#60)](https://github.com/aikatore/recvx-enhanced-port/issues/60).
Both F1 and OPTION > ENHANCED OPTIONS use the same settings table and binding
editors. No individual settings are hidden by selecting a preset.

Saturation is in Display alongside brightness, contrast and gamma: 0% is
grayscale, 100% preserves the original colour, and 200% strengthens chroma.
It follows tone correction, preserves neutral grays, and clamps the final colour
range. Reset picture calibration restores brightness 0, contrast 100%, gamma
1.0 and saturation 100%. Anti-aliasing remains a separate choice.

Optional FXAA smooths edges in the final game image. It uses the physical source
texture size, so internal resolution can be adjusted independently. Every filter
tap stays within the displayed crop. Both presets leave FXAA off: it can soften
game lettering, textures, grain and other fine detail. Higher internal resolution
continues to provide its existing quality option. The original game UI and
native Enhanced Options are part of the filtered image; F1 is drawn afterwards.

Experience preset is the first setting on Display and Gameplay:

| Controlled preference | Original | Enhanced |
|---|---|---|
| Aspect / internal resolution | 4:3 / 1x | Widescreen / Auto |
| Display-rate frames / smooth menus | Off / Off | On / On |
| Previous-frame trail | On | Auto |
| Vignette / cutscene bars / effect framing | Original | Stretched |
| Fog / picture calibration | Original / neutral | Original / neutral |
| FXAA / movie zoom | Off / Off | Off / Off |
| Doors / loading | Original / original waits | Skippable / faster |
| Quick knife / automatic stairs | Off / Off | On / On |
| Fixed-camera movement | Classic | Modern |

Original is an original-style presentation and control baseline. Renderer
backend, HD packs, culling, third-person mode and camera tuning remain separate
choices. Presets also preserve bindings, display mode, physical window size,
VSync/frame cap, audio, vibration, save configuration, progress, difficulty and
cheats. Choosing a named preset resets only the preferences in the table.

Custom preserves values. Editing a controlled preference automatically displays
Custom; restoring every value to the selected baseline recovers its name.
Explicitly choosing Custom keeps that name even if its values match a baseline.
Older configurations are retained as Custom. A saved profile marker is checked
against actual values, so manually edited configuration cannot falsely display
a named preset. All changes use the normal delayed settings save.

Verification of the Windows x86 build:

- All 24 non-movie CTest checks and nine parent source/tool/asset checks passed.
  Preset tests compare the complete configuration object to prove that unrelated
  fields survive selection, and exercise every controlled field's Custom state.
- The actual production shaders passed a hidden OpenGL 3.3 context test: saturation
  0/100/200%, gray/flat-colour preservation, tone correction order, neutral bypass,
  HDR clamp, crop isolation, physical texture-size equivalence, black letterbox
  regions, opaque alpha and no GL errors. The AA-off image remains unchanged.
- On the synthetic diagonal-edge fixture, squared error against a supersampled
  coverage reference decreased from 2.323029 to 1.472553 with FXAA, about 36.6%.
  This is a controlled image-quality result, not a general performance or quality
  guarantee. It does not replace testing internal resolution on a user's hardware.
- F1 and native Enhanced Options captures show the shared preset row. The opening
  campaign completed a 2400-frame run while switching Original to Enhanced,
  desaturating, restoring colour and enabling AA; movement continued normally.
  Both new picture controls saved successfully. Evidence is under
  `local/campaign-smoke/QualityOverlay`, `QualityNative` and `QualityGameplay`.
- A fresh process reloaded the saved Custom picture, reset calibration without
  changing AA, recovered Enhanced when AA was turned off, then explicitly chose
  and saved Custom. Evidence: `local/campaign-smoke/QualityReload`.

The current executable is `build/campaign-platform/dist-quality/AshfordsHalberd.exe`.
SHA256: `25AA40FAB5FEF45F122DF8B03D1E251004288EFD58D4F06B386120EC261F85B8`.
The full campaign remains unverified; movie decoding was unchanged and its earlier
full-disc validation was not repeated for this update.
