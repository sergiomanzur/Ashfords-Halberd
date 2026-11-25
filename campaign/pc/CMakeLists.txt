# PC port build.
#
#   recvx_game     : the original game + platform code from the decomp, compiled
#                    unmodified except for '#ifdef TARGET_PC' asm replacements.
#   recvx_platform : the new PC abstraction layer (PS2 hardware/IOP emulation at
#                    library boundaries, renderer, audio, input, overlay).
#   recvx          : the executable.

set(DECOMP ${RECVX_DECOMP_DIR})
add_compile_definitions(_FILE_OFFSET_BITS=64)   # disc images over 2 GB (cdvd.c fseeko); mingw-w64 honours it too
if(WIN32)
    # the crash log walks the EBP frame chain (host/log.c); measured on Linux: +1..5% game logic
    add_compile_options(-fno-omit-frame-pointer)
endif()
# SSE flags are x86-only; arm64 uses the portable C paths.
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(RECVX_SSE_FLAGS "")
else()
    set(RECVX_SSE_FLAGS -msse2 -mfpmath=sse)
endif()

set(PROG ${DECOMP}/src/ps2/veronica/prog)
set(KATANA_INC ${DECOMP}/include/recvx-decomp-katana/KATANA/Include)

# SDL3 is built from source and linked statically by default, so the binary does not
# depend on a (32-bit) SDL3 package being installed. SDL still dlopen()s the system's
# X11/Wayland/audio libraries at runtime.
option(RECVX_VENDORED_SDL "Build SDL3 from source and link it statically" ON)
if(RECVX_VENDORED_SDL)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git GIT_TAG release-3.4.16 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 REQUIRED CONFIG)
endif()

# Dear ImGui (MIT) for the F1 overlay, with its SDL3 + OpenGL3 backends.
include(FetchContent)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(imgui PUBLIC GLESv3)
endif()

# ---------------------------------------------------------------- headers
# The Katana SDK headers are included with inconsistent case (SG_MWPLY.H vs
# sg_mwply.h). Mirror them under both spellings for case-sensitive filesystems.
set(CASEFOLD_DIR ${CMAKE_BINARY_DIR}/katana_casefold)
file(GLOB KATANA_HEADERS RELATIVE ${KATANA_INC} ${KATANA_INC}/*)
foreach(h ${KATANA_HEADERS})
    string(TOLOWER ${h} lo)
    string(TOUPPER ${h} up)
    foreach(n ${lo} ${up})
        if(NOT n STREQUAL h AND NOT EXISTS ${KATANA_INC}/${n} AND NOT EXISTS ${CASEFOLD_DIR}/${n})
            file(MAKE_DIRECTORY ${CASEFOLD_DIR})
            file(CREATE_LINK ${KATANA_INC}/${h} ${CASEFOLD_DIR}/${n} COPY_ON_ERROR SYMBOLIC)
        endif()
    endforeach()
endforeach()

# ---------------------------------------------------------------- game (decomp)
file(GLOB GAME_SOURCES ${PROG}/*.c)
set(GAME_EXCLUDE
    gcc_wrapper.c        # ee-gcc soft-float/int64 runtime helpers
    ps2_MovieFunc.c      # libmpeg/IPU movie player -> pc/src/movie
    ps2_MovieWork.c
    ps2_Vu1Scissor2.c    # #included by ps2_Vu1Strip.c
)
foreach(x ${GAME_EXCLUDE})
    list(REMOVE_ITEM GAME_SOURCES ${PROG}/${x})
endforeach()

# PC implementations of decomp-declared APIs that need the decomp's types
file(GLOB GLUE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/glue/*.c)

if(NOT ASHFORD_PLATFORM_TESTS_ONLY)
add_library(recvx_game OBJECT ${GAME_SOURCES} ${GLUE_SOURCES})
target_compile_definitions(recvx_game PRIVATE main=recvx_game_main)
target_include_directories(recvx_game PRIVATE
    ${DECOMP}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
    ${DECOMP}/src/cri/mwlib/ee/lib/libadxe
    ${PROG}
)
# Katana/CRI dirs ship libc-named headers (math.h, stddef.h, ...); search them
# after the host's system headers.
target_compile_options(recvx_game PRIVATE
    "SHELL:-idirafter \"${KATANA_INC}\""
    "SHELL:-idirafter \"${CASEFOLD_DIR}\""
    "SHELL:-idirafter \"${DECOMP}/include/recvx-decomp-cri/cri/mwlib/include\""
    "SHELL:-idirafter \"${DECOMP}/include/recvx-decomp-cri/cri/mwlib/ee/include\""
    "SHELL:-include \"${CMAKE_CURRENT_SOURCE_DIR}/include/pc_prelude.h\""
    -std=gnu11
    -fno-strict-aliasing
    -fwrapv
    -fsigned-char             # EE char is signed
    -ffp-contract=off         # no FMA fusion: keep the PS2's operation order
    ${RECVX_SSE_FLAGS}         # IEEE single precision, no x87 extended intermediates (x86 only)
    -fno-tree-vectorize
    -ftrivial-auto-var-init=zero  # deterministic stand-in for MWCC's stale registers (docs/dev/uninitialized-locals.txt)
)
# the decomp is compiled as is, without warnings (-w), but a call to an undeclared function stays an
# error: without a prototype a float argument goes as a double and a char/float return is read as an
# int. -w also hides -Werror=..., so this relies on GCC 14's permerror (no -fpermissive here; the other
# permerrors the decomp needs are switched off one by one). GCC 13 (CI) only warns: the ctest
# decomp_declarations checks it there. Missing returns and uninitialized locals are listed by
# pc/tools/decomp_warnings.py. The port's own glue gets warnings.
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Clang 17 does not recognize GCC 14's -Wno-return-mismatch; suppress the
    # matching Clang diagnostics while keeping undeclared-function errors active.
    # Disable bionic _FORTIFY_SOURCE overload wrappers on decomp TUs (e.g.
    # sub1.c's memset(ptr, NULL, len) fails __overloadable resolution).
    target_compile_options(recvx_game PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
    set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-w;-Wno-incompatible-pointer-types;-Wno-incompatible-function-pointer-types;-Wno-int-conversion;-Wno-return-type;-Wno-format-security")
else()
    set_source_files_properties(${GAME_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-w;-Wno-incompatible-pointer-types;-Wno-int-conversion;-Wno-return-mismatch")
endif()
set_source_files_properties(${GLUE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wno-unused-function")
if(WIN32)
    # GCC (not MSVC) bitfield layout, as on Linux and the EE
    target_compile_options(recvx_game PRIVATE -mno-ms-bitfields)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # the executable is non-PIE (fixed address window); position-independent code only costs
    # get_pc_thunk calls on x86-32
    target_compile_options(recvx_game PRIVATE -fno-pie)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set_target_properties(recvx_game PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Compile snapshot manifest/core and VU DMA stub packets without pc_prelude.h so
    # .ahsnap and ps2_vu*sub* live inside librecvx_game.so's [0x33000000, 0x40000000) window.
    # librecvx_game.so itself is linked below after libmain.so so DT_NEEDED includes libmain.so.
    add_library(recvx_game_snapshot OBJECT
        src/host/snapshot_manifest.c
        src/host/snapshot_core.c
        src/ps2/vu_stubs_android.c
    )
    set_target_properties(recvx_game_snapshot PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_options(recvx_game_snapshot PRIVATE -std=gnu11 -Wall)
    target_include_directories(recvx_game_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

endif()

# ---------------------------------------------------------------- platform layer
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # On Android, .ahsnap, snapshot_core, and the VU DMA stub packets live inside
    # librecvx_game.so so snapshot_manifest.py patches the game library directly and
    # Ps2Vu*ProgSend passes windowed addresses to sceDmaSend().
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "src/host/snapshot_(manifest|core)\\.c$")
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "src/ps2/vu_stubs_android\\.c$")
else()
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "src/host/android_game_bridge\\.c$")
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "src/host/android_saf\\.c$")
endif()
add_library(recvx_platform OBJECT ${PLATFORM_SOURCES})
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set_target_properties(recvx_platform PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
# xxHash (BSD-2-Clause, header only): XXH3, the hash PCSX2 names its texture replacements by (src/gs/hdpack.c)
include(FetchContent)
FetchContent_Declare(xxhash GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git GIT_TAG v0.8.3 GIT_SHALLOW TRUE SOURCE_SUBDIR none)
# bcdec (MIT / Unlicense, header only): BC1-BC7 decoding for the replacements' content check
FetchContent_Declare(bcdec GIT_REPOSITORY https://github.com/iOrange/bcdec.git GIT_TAG 80859ed3b7afb1c527a2a99d70c61457bea72d0c SOURCE_SUBDIR none)
FetchContent_MakeAvailable(xxhash bcdec)
target_include_directories(recvx_platform PRIVATE ${xxhash_SOURCE_DIR} ${bcdec_SOURCE_DIR})
# the render thread's per-vertex loops (GS emulation, native strips, interpolation): -O3 (IEEE as at
# -O2: no fast-math, -ffp-contract=off)
set_property(SOURCE src/gs/gs_gl.c src/gs/stream.c src/gs/gs_mem.c src/gs/textures.c APPEND PROPERTY COMPILE_OPTIONS -O3)
set_property(SOURCE src/host/host.c src/host/overlay.cpp APPEND PROPERTY COMPILE_DEFINITIONS "RECVX_VERSION=\"${RECVX_VERSION}\"")
set_property(SOURCE src/host/host.c APPEND PROPERTY COMPILE_DEFINITIONS "ASHFORD_VERSION=\"${ASHFORD_VERSION}\"")
set_property(SOURCE src/host/tr.c APPEND PROPERTY COMPILE_DEFINITIONS "TR_TABLE_INC=\"${CMAKE_CURRENT_SOURCE_DIR}/src/host/tr_table.inc\"")
set_property(SOURCE src/host/tr.c APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/host/tr_table.inc)
target_include_directories(recvx_platform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ps2
)
target_compile_options(recvx_platform PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    -fno-strict-aliasing -fwrapv -ffp-contract=off ${RECVX_SSE_FLAGS}
    -Wall -Wno-unused-function
)
target_link_libraries(recvx_platform PRIVATE SDL3::SDL3 imgui)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(recvx_platform PRIVATE -fno-pie)
endif()
if(WIN32)
    target_compile_options(recvx_platform PRIVATE $<$<COMPILE_LANGUAGE:C>:-mno-ms-bitfields>)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # tr.c's inline-asm .trtab symbols are global; make them hidden so arm64 uses
    # PC-relative addressing instead of GOT (which requires 8-byte section alignment).
    set(_android_tr_visibility "${CMAKE_BINARY_DIR}/android_tr_visibility.h")
    file(WRITE "${_android_tr_visibility}"
        "#ifndef ANDROID_TR_VISIBILITY_H\n"
        "#define ANDROID_TR_VISIBILITY_H\n"
        "__attribute__((visibility(\"hidden\"))) extern const char tr_table_text[];\n"
        "__attribute__((visibility(\"hidden\"))) extern const char tr_table_end[];\n"
        "#endif\n")
    set_source_files_properties(src/host/tr.c PROPERTIES COMPILE_OPTIONS "-include${_android_tr_visibility}")
    # arm64 Android devices use 16 KiB max page sizes; match it for segment layout.
    add_link_options("-Wl,-z,max-page-size=16384")
endif()

# ---------------------------------------------------------------- executable
set(RECVX_LAYOUT_PAD 0 CACHE STRING "test aid: bytes of padding before all code, data and bss (pc/tools/layout_check.sh)")
if(RECVX_LAYOUT_PAD AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(LAYOUT_PAD_SRC tests/layout_pad.c)
    set_source_files_properties(tests/layout_pad.c PROPERTIES COMPILE_DEFINITIONS RECVX_LAYOUT_PAD=${RECVX_LAYOUT_PAD})
endif()
# Only original Ashford artwork is used for the launcher and executable icon.
set(ASHFORD_BRANDING ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/branding)
if(WIN32)
    enable_language(RC)
    file(TO_CMAKE_PATH "${ASHFORD_BRANDING}/ashford.ico" ASHFORD_ICON_PATH)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/host/tr_table.inc" TR_TABLE_PATH)
    configure_file(src/host/ashford.rc.in "${CMAKE_CURRENT_BINARY_DIR}/ashford.rc" @ONLY)
    # Keep RC isolated from SDL/ImGui's include paths. windres reconstructs its
    # preprocessor command incorrectly for inherited paths with apostrophes.
    add_library(ashford_icon OBJECT "${CMAKE_CURRENT_BINARY_DIR}/ashford.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/ashford.rc" PROPERTIES OBJECT_DEPENDS
        "${ASHFORD_BRANDING}/ashford.ico;${CMAKE_CURRENT_SOURCE_DIR}/src/host/tr_table.inc")
    set(RECVX_RES $<TARGET_OBJECTS:ashford_icon>)
endif()
if(NOT ASHFORD_PLATFORM_TESTS_ONLY AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
add_executable(recvx ${LAYOUT_PAD_SRC} ${RECVX_RES} $<TARGET_OBJECTS:recvx_game> $<TARGET_OBJECTS:recvx_platform>
               $<$<TARGET_EXISTS:glibc_compat>:$<TARGET_OBJECTS:glibc_compat>>)
target_link_libraries(recvx PRIVATE imgui SDL3::SDL3 m)
target_link_libraries(recvx PRIVATE ashford_campaign_disc)
set_target_properties(recvx PROPERTIES OUTPUT_NAME AshfordsHalberd WIN32_EXECUTABLE ${WIN32})
add_custom_command(TARGET recvx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:recvx>/assets/branding"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASHFORD_BRANDING}/launcher.bmp" "${ASHFORD_BRANDING}/ashford-icon.bmp"
        "${ASHFORD_BRANDING}/launcher.wav" "$<TARGET_FILE_DIR:recvx>/assets/branding")
if(WIN32)
    target_link_libraries(recvx PRIVATE pthread shell32 dwmapi)   # clock_gettime (winpthreads); borderless corners (settings.c)
    # Same address window as Linux: fixed image base, no ASLR relocation; large-address-aware
    # gives the 32-bit exe 4 GB on 64-bit Windows (Linux already uses addresses above 2 GB). -static: libstdc++,
    # libgcc and winpthread go into the exe, only system DLLs (UCRT, opengl32, ...) remain.
    target_link_options(recvx PRIVATE -static -Wl,--image-base,0x30000000 -Wl,--disable-dynamicbase -Wl,--large-address-aware
                        -Wl,--stack,8388608 "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/snapshot.map")
    add_custom_command(TARGET recvx POST_BUILD
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/snapshot_manifest.py
            --exe "$<TARGET_FILE:recvx>" --map "${CMAKE_CURRENT_BINARY_DIR}/snapshot.map"
            --nm "${CMAKE_NM}" --audit "${CMAKE_CURRENT_BINARY_DIR}/snapshot-manifest.json"
        VERBATIM)
endif()

install(TARGETS recvx RUNTIME DESTINATION .)
install(FILES "${ASHFORD_BRANDING}/launcher.bmp" "${ASHFORD_BRANDING}/ashford-icon.bmp"
    "${ASHFORD_BRANDING}/launcher.wav" "${ASHFORD_BRANDING}/ashford.ico"
    "${ASHFORD_BRANDING}/PROVENANCE.md" "${ASHFORD_BRANDING}/prompts.txt" DESTINATION assets/branding)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE DESTINATION licenses RENAME recvx-enhanced-port-LICENSE.txt)
install(FILES ${DECOMP}/LICENSE DESTINATION licenses RENAME recvx-decomp-LICENSE.txt)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../PROVENANCE.md ${CMAKE_CURRENT_SOURCE_DIR}/../THIRD_PARTY_NOTICES.md DESTINATION licenses)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../docs/campaign-package-readme.md DESTINATION . RENAME README.md)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../CHANGELOG.md DESTINATION .)
install(FILES ${sdl3_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME SDL3-LICENSE.txt)
install(FILES ${imgui_SOURCE_DIR}/LICENSE.txt DESTINATION licenses RENAME ImGui-LICENSE.txt)
install(FILES ${xxhash_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME xxHash-LICENSE.txt)
install(FILES ${bcdec_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME bcdec-LICENSE.txt)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET recvx)
    # Place the image inside the PS2 address window (see docs/ARCHITECTURE.md, "Data model and memory").
    target_link_options(recvx PRIVATE -no-pie -Wl,-Ttext-segment=0x30000000 -static-libgcc -static-libstdc++)
    set_target_properties(recvx PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

# ---------------------------------------------------------------- tests
add_library(ashford_campaign_disc STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core.cpp)
target_compile_features(ashford_campaign_disc PUBLIC cxx_std_17)
target_include_directories(ashford_campaign_disc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
if(WIN32)
    target_link_libraries(ashford_campaign_disc PUBLIC bcrypt)
endif()
add_executable(test_ashford_disc tests/test_ashford_disc.cpp src/host/ashford_disc.cpp)
target_include_directories(test_ashford_disc PRIVATE src)
target_link_libraries(test_ashford_disc PRIVATE ashford_campaign_disc)
add_test(NAME ashford_disc COMMAND test_ashford_disc)

add_executable(test_ashford_panel tests/test_ashford_panel.cpp src/host/ashford_panel.cpp)
target_include_directories(test_ashford_panel PRIVATE src)
target_link_libraries(test_ashford_panel PRIVATE imgui)
add_test(NAME ashford_panel COMMAND test_ashford_panel)

add_executable(test_vu tests/test_vu.c src/ps2/vu.c)
target_compile_options(test_vu PRIVATE -std=gnu11 ${RECVX_SSE_FLAGS} -ffp-contract=off)
target_link_libraries(test_vu PRIVATE m)
add_test(NAME vu COMMAND test_vu)
# the same against the plain C (non-SSE) native paths, as an arm64 build would compile them
add_executable(test_vu_portable tests/test_vu.c src/ps2/vu.c)
target_compile_definitions(test_vu_portable PRIVATE RECVX_VU_PORTABLE)
target_compile_options(test_vu_portable PRIVATE -std=gnu11 ${RECVX_SSE_FLAGS} -ffp-contract=off)
target_link_libraries(test_vu_portable PRIVATE m)
add_test(NAME vu_portable COMMAND test_vu_portable)

# decodes every movie on the disc (needs RECVX_ISO; skipped otherwise)
add_executable(test_movies tests/test_movies.c src/movie/mpeg2.c src/movie/pss.c)
target_compile_options(test_movies PRIVATE -std=gnu11 -O2)
target_link_libraries(test_movies PRIVATE m)
add_test(NAME movies COMMAND test_movies)
set_tests_properties(movies PROPERTIES SKIP_RETURN_CODE 77)

# calls to undeclared functions in the decomp: GCC 14+ stops the build on them, GCC 13 only warns (and -w hides it)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND AND CMAKE_GENERATOR STREQUAL "Ninja" AND NOT ASHFORD_PLATFORM_TESTS_ONLY)
    add_test(NAME decomp_declarations COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/decomp_warnings.py --declarations ${CMAKE_BINARY_DIR})
endif()

# control bindings (pc/src/input/bindings.c): defaults, rebinding, ini, a virtual SDL gamepad
add_executable(test_bindings tests/test_bindings.c src/input/bindings.c src/input/tps_input.c)
target_compile_options(test_bindings PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=gnu11>)
target_link_libraries(test_bindings PRIVATE SDL3::SDL3)
add_test(NAME bindings COMMAND test_bindings)
set(PAD_WEAPON_SWITCH_TEST ${CMAKE_CURRENT_BINARY_DIR}/pad_weapon_switch_test.inc)
add_custom_command(OUTPUT "${PAD_WEAPON_SWITCH_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pad.c "${PAD_WEAPON_SWITCH_TEST}" pad_weapon_switch
    DEPENDS tests/control_source.py tests/reload_source.py src/input/pad.c VERBATIM)
add_executable(test_pad_weapon_switch tests/test_pad_weapon_switch.c src/input/bindings.c "${PAD_WEAPON_SWITCH_TEST}")
target_include_directories(test_pad_weapon_switch PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(test_pad_weapon_switch PRIVATE -std=gnu11)
target_link_libraries(test_pad_weapon_switch PRIVATE SDL3::SDL3)
add_test(NAME pad_weapon_switch COMMAND test_pad_weapon_switch)
set(PAD_QUICKSAVE_TEST ${CMAKE_CURRENT_BINARY_DIR}/pad_quicksave_test.inc)
add_custom_command(OUTPUT "${PAD_QUICKSAVE_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pad.c "${PAD_QUICKSAVE_TEST}" ensure_gamepad pad_quicksave_actions
    DEPENDS tests/control_source.py tests/reload_source.py src/input/pad.c VERBATIM)
add_executable(test_pad_quicksave tests/test_pad_quicksave.c src/input/bindings.c "${PAD_QUICKSAVE_TEST}")
target_include_directories(test_pad_quicksave PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(test_pad_quicksave PRIVATE -std=gnu11)
target_link_libraries(test_pad_quicksave PRIVATE SDL3::SDL3)
add_test(NAME pad_quicksave COMMAND test_pad_quicksave)

# modern controls' direction math (pc/src/input/modern.c): camera + stick -> angle, the cut hold
add_executable(test_modern tests/test_modern.c src/input/modern.c)
target_compile_options(test_modern PRIVATE -std=gnu11)
target_link_libraries(test_modern PRIVATE m)
add_test(NAME modern COMMAND test_modern)
add_executable(test_presets tests/test_presets.c src/host/presets.c)
target_include_directories(test_presets PRIVATE include/sdk src src/ps2)
target_compile_options(test_presets PRIVATE -std=gnu11)
add_test(NAME presets COMMAND test_presets)
add_executable(test_picture_gl tests/test_picture_gl.c src/render/gl_loader.c)
target_include_directories(test_picture_gl PRIVATE include/sdk src src/ps2)
target_compile_options(test_picture_gl PRIVATE -std=gnu11)
target_link_libraries(test_picture_gl PRIVATE SDL3::SDL3 m)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(test_picture_gl PRIVATE EGL dl)
endif()
add_test(NAME picture_gl COMMAND test_picture_gl)
add_executable(test_pacing tests/test_pacing.c)
add_test(NAME pacing COMMAND test_pacing)
set(RUMBLE_TEST ${CMAKE_CURRENT_BINARY_DIR}/rumble_test.inc)
add_custom_command(OUTPUT "${RUMBLE_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/input/pad.c "${RUMBLE_TEST}" pad_can_rumble rumble_off scePadSetActDirect
    DEPENDS tests/control_source.py tests/reload_source.py src/input/pad.c VERBATIM)
add_executable(test_rumble tests/test_rumble.c "${RUMBLE_TEST}")
target_include_directories(test_rumble PRIVATE ${CMAKE_CURRENT_BINARY_DIR} include/sdk src src/ps2)
target_link_libraries(test_rumble PRIVATE SDL3::SDL3)
add_test(NAME rumble COMMAND test_rumble)

if(NOT ASHFORD_PLATFORM_TESTS_ONLY)
set(MOVIE_RETURN_TEST ${CMAKE_CURRENT_BINARY_DIR}/movie_return_test.inc)
add_custom_command(OUTPUT "${MOVIE_RETURN_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/movie_source.py
        ${PROG}/sdfunc.c "${MOVIE_RETURN_TEST}"
    DEPENDS tests/movie_source.py tests/reload_source.py ${PROG}/sdfunc.c VERBATIM)
add_executable(test_movie_return tests/test_movie_return.c "${MOVIE_RETURN_TEST}")
target_include_directories(test_movie_return PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(test_movie_return PRIVATE -std=gnu11)
add_test(NAME movie_return COMMAND test_movie_return)
set(RELOAD_GAME_TEST ${CMAKE_CURRENT_BINARY_DIR}/reload_game_test.c)
add_custom_command(OUTPUT "${RELOAD_GAME_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/reload_source.py
        ${PROG}/sub1.c "${RELOAD_GAME_TEST}"
    DEPENDS tests/reload_source.py ${PROG}/sub1.c ${PROG}/weapon.c ${PROG}/player.c
    VERBATIM)
add_executable(test_reload tests/test_reload.c "${RELOAD_GAME_TEST}" ${PROG}/item.c glue/reload_glue.c)
add_executable(test_weapon_switch tests/test_weapon_switch.c ${PROG}/item.c glue/weapon_switch_glue.c)
set(QUICK_IN_PLAY_TEST ${CMAKE_CURRENT_BINARY_DIR}/quick_in_play_test.inc)
add_custom_command(OUTPUT "${QUICK_IN_PLAY_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/reload_glue.c "${QUICK_IN_PLAY_TEST}" pc_in_play
    DEPENDS tests/control_source.py tests/reload_source.py glue/reload_glue.c VERBATIM)
add_executable(test_quicksave tests/test_quicksave.c glue/quicksave_glue.c "${QUICK_IN_PLAY_TEST}")
target_include_directories(test_quicksave PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_executable(test_modern_glue tests/test_modern_glue.c glue/modern_glue.c src/input/modern.c)
set(OPTIONS_MOUSE_TEST ${CMAKE_CURRENT_BINARY_DIR}/options_mouse_test.inc)
add_custom_command(OUTPUT "${OPTIONS_MOUSE_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/options_glue.c "${OPTIONS_MOUSE_TEST}" pc_options_mouse0
    DEPENDS tests/control_source.py tests/reload_source.py glue/options_glue.c VERBATIM)
add_executable(test_options_mouse tests/test_options_mouse.c "${OPTIONS_MOUSE_TEST}")
target_include_directories(test_options_mouse PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set(TPS_PAD_TEST ${CMAKE_CURRENT_BINARY_DIR}/tps_pad_test.inc)
add_custom_command(OUTPUT "${TPS_PAD_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/tps_glue.c "${TPS_PAD_TEST}" tps_pad
    DEPENDS tests/control_source.py tests/reload_source.py glue/tps_glue.c VERBATIM)
add_executable(test_tps_pad tests/test_tps_pad.c "${TPS_PAD_TEST}")
target_include_directories(test_tps_pad PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
foreach(target test_reload test_weapon_switch test_quicksave test_modern_glue test_options_mouse test_tps_pad)
    target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:recvx_game,INCLUDE_DIRECTORIES>)
    target_compile_options(${target} PRIVATE $<TARGET_PROPERTY:recvx_game,COMPILE_OPTIONS>)
    target_link_libraries(${target} PRIVATE m)
endforeach()
add_test(NAME reload COMMAND test_reload)
add_test(NAME weapon_switch COMMAND test_weapon_switch)
add_test(NAME quicksave COMMAND test_quicksave)
add_test(NAME modern_glue COMMAND test_modern_glue)
add_test(NAME options_mouse COMMAND test_options_mouse)
add_test(NAME tps_pad COMMAND test_tps_pad)
endif()

# pure rules of the PC hooks (pc/include/pc_rules.h, src/gs/native.h): added enemies' helper code and holders,
# dual weapons' second target, hor+ margin meshes and clipping, warp music, "Unlock all", co-op inventory blocks;
# with the game's flags and types
if(NOT ASHFORD_PLATFORM_TESTS_ONLY)
add_executable(test_rules tests/test_rules.c)
target_include_directories(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,INCLUDE_DIRECTORIES>)
target_compile_options(test_rules PRIVATE $<TARGET_PROPERTY:recvx_game,COMPILE_OPTIONS>)
target_link_libraries(test_rules PRIVATE m)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(test_rules PRIVATE -no-pie)   # (the game's flags include -fno-pie)
endif()
add_test(NAME rules COMMAND test_rules)
endif()

# every recvx.ini key has a row in the settings table (F1 and OPTION > ENHANCED SETTINGS) or is listed
add_test(NAME settings COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_settings.cmake)

# native texture queue (pc/src/gs/textures.c): superseded uploads never written, the rest only when read
add_executable(test_ntex tests/test_ntex.c src/gs/textures.c)
target_compile_options(test_ntex PRIVATE -std=gnu11)
add_test(NAME ntex COMMAND test_ntex)

# GS memory (pc/src/gs/gs_mem.c): texture decodes, read_rect and CLUT loads against per-pixel reads
# (it #includes gs_mem.c: built with -O3 to catch aggressive optimizer issues on i686)
add_executable(test_gsmem tests/test_gsmem.c)
target_compile_options(test_gsmem PRIVATE -std=gnu11 -fno-strict-aliasing -fwrapv -ffp-contract=off ${RECVX_SSE_FLAGS} -O3)
target_include_directories(test_gsmem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/sdk ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME gsmem COMMAND test_gsmem)

# SMAA's lookup textures computed at start (src/gs/smaa_tables.h): the bytes of SMAA's own tables
add_executable(test_aa_tables tests/test_aa_tables.c)
target_compile_options(test_aa_tables PRIVATE -std=gnu11 ${RECVX_SSE_FLAGS} -ffp-contract=off)
target_link_libraries(test_aa_tables PRIVATE m)
add_test(NAME aa_tables COMMAND test_aa_tables)

# checked file reading (include/pc_parse.h): PRS against the game's decoder, crafted AFS tables, .cvxm
# models, DDS / PNG headers, message tables, language parts and start-kit scripts
add_executable(test_parsers tests/test_parsers.c src/host/parse.c glue/script_rules.c)
target_compile_options(test_parsers PRIVATE -std=gnu11)
target_link_libraries(test_parsers PRIVATE m)
add_test(NAME parsers COMMAND test_parsers)

# "You appear as" list (pc/src/host/actors.c): event doubles hidden, indices kept
add_executable(test_appear tests/test_appear.c src/host/actors.c)
add_test(NAME appear COMMAND test_appear)

# enemy variants (pc/src/host/variants.c): the built-in table, the mods/enemies.ini reader
add_executable(test_variants tests/test_variants.c src/host/variants.c src/host/actors.c src/host/fileio.c)
add_test(NAME variants COMMAND test_variants)

# UTF-8 file paths and replace-on-rename saving (pc/src/host/fileio.c)
add_executable(test_fileio tests/test_fileio.c src/host/fileio.c)
target_compile_options(test_fileio PRIVATE -std=gnu11)
add_test(NAME fileio COMMAND test_fileio)

# native save files (pc/src/host/savestore.c)
add_executable(test_savestore tests/test_savestore.c src/host/savestore.c src/host/fileio.c)
target_compile_options(test_savestore PRIVATE -std=gnu11)
target_link_libraries(test_savestore PRIVATE SDL3::SDL3)
add_test(NAME savestore COMMAND test_savestore)
add_executable(test_quickstore tests/test_quickstore.c src/host/quickstore.c src/host/fileio.c)
target_compile_options(test_quickstore PRIVATE -std=gnu11)
target_link_libraries(test_quickstore PRIVATE SDL3::SDL3)
add_test(NAME quickstore COMMAND test_quickstore)
add_executable(test_snapshot_store tests/test_snapshot_store.c src/host/snapshot_store.c src/host/fileio.c)
target_compile_options(test_snapshot_store PRIVATE -std=gnu11)
target_link_libraries(test_snapshot_store PRIVATE SDL3::SDL3)
add_test(NAME snapshot_store COMMAND test_snapshot_store)
add_executable(test_snapshot_core tests/test_snapshot_core.c src/host/snapshot_core.c)
target_compile_options(test_snapshot_core PRIVATE -std=gnu11)
target_compile_definitions(test_snapshot_core PRIVATE SNAPSHOT_TEST_PATCHED=1)
add_test(NAME snapshot_core COMMAND test_snapshot_core)
add_executable(test_snapshot_unpatched tests/test_snapshot_core.c src/host/snapshot_core.c src/host/snapshot_manifest.c)
target_compile_options(test_snapshot_unpatched PRIVATE -std=gnu11)
add_test(NAME snapshot_unpatched COMMAND test_snapshot_unpatched)
add_test(NAME snapshot_manifest COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_snapshot_manifest.py)
add_executable(test_game_rng tests/test_game_rng.c src/ps2/stubs.c)
target_include_directories(test_game_rng PRIVATE src include/sdk)
target_link_libraries(test_game_rng PRIVATE SDL3::SDL3)
add_test(NAME game_rng COMMAND test_game_rng)
set(CHEAT_ROOM_TEST ${CMAKE_CURRENT_BINARY_DIR}/cheat_room_test.inc)
add_custom_command(OUTPUT "${CHEAT_ROOM_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/cheat_glue.c "${CHEAT_ROOM_TEST}"
        cheat_room_snapshot_capture cheat_room_snapshot_validate cheat_room_snapshot_restore pc_noclip
    DEPENDS tests/control_source.py tests/reload_source.py glue/cheat_glue.c VERBATIM)
add_executable(test_cheat_room_snapshot tests/test_cheat_room_snapshot.c "${CHEAT_ROOM_TEST}")
target_include_directories(test_cheat_room_snapshot PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(test_cheat_room_snapshot PRIVATE -std=gnu11)
add_test(NAME cheat_room_snapshot COMMAND test_cheat_room_snapshot)
set(FIRST_PERSON_TEST ${CMAKE_CURRENT_BINARY_DIR}/first_person_test.inc)
add_custom_command(OUTPUT "${FIRST_PERSON_TEST}"
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tests/control_source.py
        ${CMAKE_CURRENT_SOURCE_DIR}/glue/tps_glue.c "${FIRST_PERSON_TEST}"
        camera_allowed tps_first_person_active first_person_eye_height view
    DEPENDS tests/control_source.py tests/reload_source.py glue/tps_glue.c VERBATIM)
add_executable(test_first_person tests/test_first_person.c "${FIRST_PERSON_TEST}")
target_include_directories(test_first_person PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(test_first_person PRIVATE m)
add_test(NAME first_person COMMAND test_first_person)
add_executable(test_renderer_snapshot tests/test_renderer_snapshot.c src/gs/aa.c src/gs/gs.c src/gs/gs_mem.c src/gs/textures.c src/gs/stream.c src/ps2/vif.c src/ps2/vu.c src/render/gl_loader.c)
target_include_directories(test_renderer_snapshot PRIVATE src src/ps2 include/sdk)
target_compile_options(test_renderer_snapshot PRIVATE -std=gnu11)
target_link_libraries(test_renderer_snapshot PRIVATE SDL3::SDL3 m)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(test_renderer_snapshot PRIVATE EGL dl)
endif()
add_test(NAME renderer_snapshot COMMAND test_renderer_snapshot)
add_executable(test_renderer_snapshot_codec tests/test_renderer_snapshot_codec.c)
target_include_directories(test_renderer_snapshot_codec PRIVATE src)
add_test(NAME renderer_snapshot_codec COMMAND test_renderer_snapshot_codec)
# cutscene cast and voice-line randomizer (pc/src/host/cast.c, voices.c)
add_executable(test_cast tests/test_cast.c src/host/cast.c src/host/voices.c src/host/actors.c glue/script_rules.c)
target_compile_options(test_cast PRIVATE -std=gnu11)
add_test(NAME cast COMMAND test_cast)

# extra item boxes, blue herb planters and typewriter placements (pc/src/host/placer.c, extras.c)
add_executable(test_placer tests/test_placer.c src/host/placer.c src/host/extras.c src/host/fileio.c)
target_compile_options(test_placer PRIVATE -std=gnu11)
target_link_libraries(test_placer PRIVATE m)
add_test(NAME placer COMMAND test_placer)

# BioRand options / seed string encoder-decoder (pc/src/host/biorand_opts.c)
add_executable(test_biorand_opts tests/test_biorand_opts.c src/host/biorand_opts.c)
target_compile_options(test_biorand_opts PRIVATE -std=gnu11)
target_link_libraries(test_biorand_opts PRIVATE m)
add_test(NAME biorand_opts COMMAND test_biorand_opts)

# BioRand spoiler log parser (pc/src/host/spoiler.c)
add_executable(test_spoiler tests/test_spoiler.c src/host/spoiler.c)
target_compile_options(test_spoiler PRIVATE -std=gnu11)
add_test(NAME spoiler COMMAND test_spoiler)

# Outbreak File #2 & language disc import (pc/src/import/*)
add_executable(test_import tests/test_import.c src/import/impmodel.c src/import/ob2.c src/import/import.c src/import/lang.c src/host/glyphs.c src/host/actors.c src/host/fileio.c src/host/parse.c)
target_compile_options(test_import PRIVATE -std=gnu11)
target_link_libraries(test_import PRIVATE SDL3::SDL3 m)
add_test(NAME import COMMAND test_import ${CMAKE_CURRENT_BINARY_DIR})

# Language fonts, Arabic shaping, CJK wrapping, and translation table (pc/src/host/glyphs.c, tr.c, tr_table.inc)
add_executable(test_lang tests/test_lang.c src/host/glyphs.c src/host/tr.c ${RECVX_RES})
target_compile_options(test_lang PRIVATE -std=gnu11)
add_test(NAME lang COMMAND test_lang)

set(AUDIO_SNAPSHOT_SOURCES src/audio/adx.c src/audio/snd_synth.c src/audio/snd_iop.c src/ps2/sif.c src/host/parse.c)
add_executable(test_audio_snapshot tests/test_audio_snapshot.c src/audio/audio.c ${AUDIO_SNAPSHOT_SOURCES})
add_executable(test_audio_snapshot_decoder tests/test_audio_snapshot_decoder.c ${AUDIO_SNAPSHOT_SOURCES})
add_executable(test_audio_snapshot_synth tests/test_audio_snapshot_synth.c)
foreach(target test_audio_snapshot test_audio_snapshot_decoder test_audio_snapshot_synth)
    target_include_directories(${target} PRIVATE src src/ps2 include/sdk)
    target_compile_options(${target} PRIVATE -std=gnu11)
    target_link_libraries(${target} PRIVATE SDL3::SDL3 m)
    add_test(NAME ${target} COMMAND ${target})
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    # Instrumentation test libraries (libjni_test_<name>.so) for the Android
    # Java-lifecycle-dependent SDL tests. These tests call SDL_Init(SDL_INIT_GAMEPAD/AUDIO),
    # which dereferences SDL's JavaVM initialized from a Java process; headless
    # adb shell runs segfault on mJavaVM == NULL. Each library embeds the same
    # sources as the corresponding test_<name> executable, exposes the test main
    # as a com.ashfordshalberd.app.NativeSDLTests JNI method, and links SDL3
    # statically with JNI_OnLoad exported so System.loadLibrary() bootstraps the
    # JavaVM inside the androidTest process. The libs are staged ONLY into the
    # androidTest APK so app-debug.apk stays lean.
    function(ashford_add_jni_test name)
        set(extra_sources ${ARGN})
        add_library(jni_test_${name} SHARED tests/jni_test_${name}.c ${extra_sources})
        target_compile_options(jni_test_${name} PRIVATE -std=gnu11)
        target_link_libraries(jni_test_${name} PRIVATE SDL3::SDL3 m)
        target_link_options(jni_test_${name} PRIVATE
            "-Wl,-u,JNI_OnLoad"
            "-Wl,-z,max-page-size=16384"
        )
    endfunction()

    ashford_add_jni_test(bindings src/input/bindings.c src/input/tps_input.c)
    ashford_add_jni_test(pad_weapon_switch src/input/bindings.c "${PAD_WEAPON_SWITCH_TEST}")
    target_include_directories(jni_test_pad_weapon_switch PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    ashford_add_jni_test(pad_quicksave src/input/bindings.c "${PAD_QUICKSAVE_TEST}")
    target_include_directories(jni_test_pad_quicksave PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    ashford_add_jni_test(rumble "${RUMBLE_TEST}")
    target_include_directories(jni_test_rumble PRIVATE ${CMAKE_CURRENT_BINARY_DIR} include/sdk src src/ps2)
    ashford_add_jni_test(audio_snapshot src/audio/audio.c ${AUDIO_SNAPSHOT_SOURCES})
    target_include_directories(jni_test_audio_snapshot PRIVATE src src/ps2 include/sdk)
    ashford_add_jni_test(audio_snapshot_decoder ${AUDIO_SNAPSHOT_SOURCES})
    target_include_directories(jni_test_audio_snapshot_decoder PRIVATE src src/ps2 include/sdk)
    ashford_add_jni_test(audio_snapshot_synth)
    target_include_directories(jni_test_audio_snapshot_synth PRIVATE src src/ps2 include/sdk)

    # Keep the on-device ctest roster honest: the Java-lifecycle tests are
    # intentionally skipped headless and verified by the androidTest harness
    # (tools/run-android-instrumentation.py) instead.
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android" AND NOT ASHFORD_PLATFORM_TESTS_ONLY)
    # Full Android application library loaded by org.libsdl.app.SDLActivity.
    # Exports SDL_main, JNI_OnLoad (via SDL3-static), and platform symbols (--export-dynamic),
    # while keeping all 61 game bridge shims hidden and omitting librecvx_game.so from DT_NEEDED.
    add_library(recvx_platform_android OBJECT ${PLATFORM_SOURCES})
    target_include_directories(recvx_platform_android PRIVATE $<TARGET_PROPERTY:recvx_platform,INCLUDE_DIRECTORIES>)
    target_compile_options(recvx_platform_android PRIVATE $<TARGET_PROPERTY:recvx_platform,COMPILE_OPTIONS>)
    target_compile_definitions(recvx_platform_android PRIVATE ASHFORD_ANDROID_GAME_BRIDGE=1
        "RECVX_VERSION=\"${RECVX_VERSION}\"" "ASHFORD_VERSION=\"${ASHFORD_VERSION}\"")
    target_link_libraries(recvx_platform_android PRIVATE SDL3::SDL3 imgui)

    set(_android_main_map "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/android-main.map")
    add_library(main SHARED $<TARGET_OBJECTS:recvx_platform_android>)
    set_target_properties(main PROPERTIES LINK_DEPENDS "${_android_main_map}")
    target_link_options(main PRIVATE
        "-Wl,--export-dynamic"
        "LINKER:--version-script,${_android_main_map}"
        "-Wl,-u,SDL_GetTicks"
        "-Wl,-u,SDL_GetTicksNS"
        "-Wl,-z,max-page-size=16384"
    )
    target_link_libraries(main PRIVATE ashford_campaign_disc SDL3::SDL3 imgui EGL GLESv3 log android m dl)

    # Windowed game shared library. Declares DT_NEEDED: libmain.so so Android bionic
    # always binds game->platform calls (g_vu0, sceDmaSend, host_log, SDL_GetTicks, ...)
    # directly to the already-loaded libmain.so even under ART's RTLD_LOCAL namespace,
    # while libmain.so never puts librecvx_game.so in DT_NEEDED (loads it at 0x33000000).
    set(_android_window_ld "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/android-window.ld")
    add_library(recvx_game_so SHARED $<TARGET_OBJECTS:recvx_game> $<TARGET_OBJECTS:recvx_game_snapshot>)
    set_target_properties(recvx_game_so PROPERTIES
        OUTPUT_NAME recvx_game
        LINK_DEPENDS "${_android_window_ld}"
    )
    target_link_options(recvx_game_so PRIVATE
        "LINKER:-T,${_android_window_ld}"
        "-Wl,-O0"
        "-Wl,-z,max-page-size=16384"
        "-Wl,-z,norelro"
        "-Wl,-Bsymbolic"
        "-Wl,--no-as-needed"
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/librecvx_game.map"
    )
    target_link_libraries(recvx_game_so PRIVATE main m)
    add_custom_command(TARGET recvx_game_so POST_BUILD
        COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/snapshot_manifest.py
            --so "$<TARGET_FILE:recvx_game_so>"
            --map "${CMAKE_CURRENT_BINARY_DIR}/librecvx_game.map"
            --nm "${CMAKE_NM}"
            --audit "${CMAKE_CURRENT_BINARY_DIR}/snapshot-manifest-android.json"
        VERBATIM)

    # On-device test harness dynamically linked against the exact libmain.so shipped in the APK.
    add_executable(test_window_load tests/test_window_load.c)
    target_include_directories(test_window_load PRIVATE src src/ps2 include/sdk)
    target_compile_options(test_window_load PRIVATE -std=gnu11 -Wall)
    target_compile_definitions(test_window_load PRIVATE ASHFORD_ANDROID_GAME_BRIDGE=1)
    set_target_properties(test_window_load PROPERTIES BUILD_RPATH "\$ORIGIN")
    target_link_libraries(test_window_load PRIVATE main dl)
    add_dependencies(test_window_load recvx_game_so)
    add_test(NAME window_load COMMAND test_window_load)
endif()

