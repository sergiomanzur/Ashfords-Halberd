# Black gameplay image after skipping the first-zombie FMV

Fixed and verified on 2026-11-02.

## Cause

The PC adaptation forced `MovieInfo.MovieFadeFlag = 1` in `PlayStartMovieEx` for
every movie. The graveyard (`RM_0020`) starts `MV_001`, whose native type is
`0x01`: skipping does not request a fade. Its event resumes player control and
reinitializes the camera without scheduling a fade-in.

The forced skip fade reached opacity 255. Movie cleanup stopped the decoder and
cleared the fade-control bit, but kept that opacity. The game therefore kept
simulating and drawing behind an opaque black overlay. The host FPS counter,
drawn afterward, remained visible. This is shared movie handling, so it affects
original, third-person and first-person cameras.

## Fix

Removed only the unconditional fade override. Dedicated PC skipping remains
enabled for every movie. The original per-movie fade bit, explicit caller fade
requests, and credits/ending overrides remain intact. No global fade reset or
camera/control workaround is applied.

The change is recorded in `campaign/patches/offline.patch` against the pinned
decomp source. It leaves game saves and user configuration untouched.

## Verification

- Reproduced the reported scene with the published 0.9.0b executable, first
  person and modern controls. The real `MV_001` decoded and accepted F8; gameplay
  resumed, but the captured game image had zero visible pixels in the sample.
- Let the same movie finish naturally in 0.9.0b: the scene remained visible.
- Added a test selecting complete production movie tables and lifecycle routines.
  It initially failed 39 of 98 checks. All 98 pass with the fix, covering all 22
  movie policies, caller-requested fades, endings and natural completion without
  clearing an unrelated scene fade.
- Ran the real graveyard skip with the fixed executable in first-person,
  original and third-person camera modes; all returned visible gameplay.
  Confirmed first-person view resumed after the skip.
- Rechecked natural movie completion with the fixed executable.
- Full campaign CTest run: 41 tests passed, no skips, including all 18 retail
  FMV decoder checks.

`tools/test-campaign-fmv-return.ps1` reproduces the scene using isolated settings,
the player's own ISO, and test-only teleport/position/infinite-health inputs.
It never sets the encounter's completion flag, uses the real decoder and F8
input, and tests the post-movie pixels rather than relying on the FPS counter.
`-WatchMovie` checks natural completion; `-Camera Original` or `ThirdPerson`
checks the other views. These targeted checks do not establish a full campaign
playthrough.

Quicksaves remain tied to their exact executable build; native saves are separate.
