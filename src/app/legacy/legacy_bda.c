#include "legacy_bda.h"
#include "legacy_keyboard.h"
#include "legacy_lowmem.h"
#include "legacy_video.h"

void legacy_bda_init(int floppy_present, int hdd_present,
                     unsigned short base_mem_kb,
                     unsigned short ebda_segment) {
    *legacy_lowmem_u16(0x0410u) = floppy_present ? 0x0001u : 0x0000u;
    *legacy_lowmem_u16(0x0413u) = base_mem_kb;
    *legacy_lowmem_u16(0x040eu) = ebda_segment;
    legacy_keyboard_init();
    legacy_video_init();
    *legacy_lowmem_u8(0x043eu) = 0x01u;
    *legacy_lowmem_u8(0x043fu) = 0x00u;
    *legacy_lowmem_u8(0x0440u) = 0x25u;
    *legacy_lowmem_u8(0x0441u) = 0x00u;
    *legacy_lowmem_u8(0x0474u) = 0x00u;
    *legacy_lowmem_u8(0x0475u) = hdd_present ? 1u : 0u;
    *legacy_lowmem_u8(0x048bu) = 0x00u;
    *legacy_lowmem_u8(0x048cu) = 0x00u;
    *legacy_lowmem_u8(0x048du) = 0x00u;
    *legacy_lowmem_u8(0x048eu) = 0x00u;
    *legacy_lowmem_u8(0x048fu) = 0x07u;
    *legacy_lowmem_u8(0x0490u) = 0x17u;
    *legacy_lowmem_u8(0x0491u) = 0x00u;
    *legacy_lowmem_u8(0x0492u) = 0x00u;
}
