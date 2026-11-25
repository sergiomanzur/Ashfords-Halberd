/* Linker script for Android arm64 librecvx_game.so.
 *
 * Loaded by window_load.c via android_dlopen_ext(ANDROID_DLEXT_RESERVED_ADDRESS)
 * into the fixed reservation [0x33000000, 0x50000000) (464 MiB), placed cleanly
 * above Android ART's 512 MiB dalvik-main space [0x12c00000, 0x32c00000).
 *
 * Because the first PT_LOAD (ro_hdr) begins at p_vaddr = 0x0 with FILEHDR PHDRS,
 * bionic computes min_vaddr = 0 and load_bias = 0x33000000 exactly:
 *   - [0x00000000, 0x00010000) -> [0x33000000, 0x33010000) ELF headers / dynsym
 *   - [0x00010000, 0x0D000000) -> [0x33010000, 0x40000000) game data, rodata,
 *                                                          .ahsnap, .trtab, bss (208 MiB)
 *   - [0x0D000000, 0x1D000000) -> [0x40000000, 0x50000000) game code (.text/.plt, 256 MiB)
 *
 * All data addresses stay inside 0x3xxxxxxx so PS2 uncached identities
 * (addr | 0x20000000 == addr, addr | 0x30000000 == addr) remain exact.
 * All segments align to 64 KiB (>= 16 KiB Android max-page-size).
 * Game .rodata lives in the RW data segment because the PS2 decomp mutates
 * string literals in place (no RO MMU protection on the EE).
 */

PHDRS
{
    phdr     PT_PHDR PHDRS FLAGS(4);         /* PF_R */
    ro_hdr   PT_LOAD FILEHDR PHDRS FLAGS(4); /* PF_R */
    data     PT_LOAD FLAGS(6);               /* PF_R | PF_W */
    text     PT_LOAD FLAGS(5);               /* PF_R | PF_X */
    dynamic  PT_DYNAMIC FLAGS(6);
    note     PT_NOTE FLAGS(4);
    eh_frame PT_GNU_EH_FRAME FLAGS(4);
    stack    PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = 0x0 + SIZEOF_HEADERS;

    /* Read-only ELF metadata at vaddr 0 so min_vaddr == 0 and load_bias == 0x30000000. */
    .note.android.ident : { *(.note.android.ident) } :ro_hdr :note
    .note.gnu.build-id  : { *(.note.gnu.build-id) }  :ro_hdr :note
    .hash               : { *(.hash) }               :ro_hdr
    .gnu.hash           : { *(.gnu.hash) }           :ro_hdr
    .dynsym             : { *(.dynsym) }             :ro_hdr
    .dynstr             : { *(.dynstr) }             :ro_hdr
    .gnu.version        : { *(.gnu.version) }        :ro_hdr
    .gnu.version_d      : { *(.gnu.version_d) }      :ro_hdr
    .gnu.version_r      : { *(.gnu.version_r) }      :ro_hdr
    .rela.dyn           : { *(.rela.dyn .rela.init .rela.text .rela.fini .rela.rodata .rela.data .rela.bss) } :ro_hdr
    .rela.plt           : { *(.rela.plt) }           :ro_hdr

    /* Window data segment: starts at >= 0x10000 (64 KiB aligned, safe for 16 KiB pages),
     * must finish strictly within the 256 MiB PS2 window (< 0x10000000 segment-relative). */
    . = MAX(0x10000, ALIGN(0x10000));
    __ashford_data_start = .;

    .dynamic        : { *(.dynamic) }                                    :data :dynamic
    .data.rel.ro    : { *(.data.rel.ro.local* .data.rel.ro .data.rel.ro.*) } :data
    .init_array     : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array .ctors))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data
    .fini_array     : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP(*(.fini_array .dtors))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data
    .got            : { *(.got) }                                        :data
    .got.plt        : { *(.got.plt) }                                    :data

    .rodata         : ALIGN(32) { *(.rodata .rodata.* .gnu.linkonce.r.*) } :data
    .ahsnap         : ALIGN(16) { KEEP(*(.ahsnap)) }                       :data
    .trtab          : ALIGN(16) { *(.trtab) }                              :data
    .data           : ALIGN(32) { *(.data .data.* .gnu.linkonce.d.*) }     :data
    .bss            : ALIGN(32) {
        *(.dynbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(32);
    } :data

    __ashford_data_end = .;
    ASSERT(__ashford_data_end <= 0x0D000000,
           "librecvx_game.so data/bss exceeds 208 MiB Android PS2 window [0x33000000, 0x40000000)")

    /* Executable code segment: fixed at vaddr 0x0D000000 -> runtime [0x40000000, 0x50000000). */
    . = 0x0D000000;
    __ashford_code_start = .;

    .init           : { KEEP(*(SORT_NONE(.init))) }                      :text
    .plt            : ALIGN(16) { *(.plt .plt.*) }                       :text
    .text           : ALIGN(16) { *(.text.unlikely .text.*_unlikely .text.exit .text.exit.* .text.startup .text.startup.* .text.hot .text.hot.* .text .stub .text.* .gnu.linkonce.t.*) } :text
    .fini           : { KEEP(*(SORT_NONE(.fini))) }                      :text
    .eh_frame_hdr   : ALIGN(4)  { *(.eh_frame_hdr .eh_frame_entry .eh_frame_entry.*) } :text :eh_frame
    .eh_frame       : ALIGN(8)  { KEEP(*(.eh_frame .eh_frame.*)) }       :text
    .gcc_except_table : ALIGN(4) { *(.gcc_except_table .gcc_except_table.*) } :text

    __ashford_code_end = .;
    ASSERT(__ashford_code_end <= 0x1D000000,
           "librecvx_game.so code exceeds 256 MiB code range [0x40000000, 0x50000000)")

    /DISCARD/ : { *(.note.GNU-stack) }
}
