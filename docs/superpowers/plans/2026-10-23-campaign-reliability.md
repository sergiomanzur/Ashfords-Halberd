# Campaign reliability: reload, input and frame pacing

Investigate reports #62, #60 and #41 against the current offline campaign.
Preserve unrelated prototype changes and the pinned engine attribution.

1. Reproduce reload behavior using actual game inventory/combine code across
   weapons, ammunition positions and normal/expanded inventories; fix confirmed
   causes and retain a regression matrix.
2. Reproduce puzzle direction, item-box steering, running push, vibration-menu
   and staircase/camera behavior; fix discrete confirmed faults with focused tests.
3. Measure the same scene in windowed, borderless and exclusive fullscreen,
   with/without VSync. Keep baseline traces, investigate timing discontinuities,
   test scheduling behavior deterministically and repeat comparable measurements.
4. Review changes, rebuild/package and report measured results separately from
   scenes and hardware that could not be validated.

Reload and input changes are delegated independently. The parent owns frame
pacing, shared build wiring, integrated runtime runs and documentation.
