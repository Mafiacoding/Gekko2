/*
 * ee_elf_loader.c - see include/core/ee_elf_loader.h for scope/
 * citations (Round 171, task #172 continuation).
 */
#include "core/ee_elf_loader.h"
#include <string.h>

static inline uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int image_range_ok(uint32_t off, uint32_t len, uint32_t image_size)
{
    return off <= image_size && len <= image_size - off;
}

static uint32_t img_u32(const uint8_t *image, uint32_t image_size, uint32_t off, int *ok)
{
    if (!image_range_ok(off, 4u, image_size)) { *ok = 0; return 0; }
    return rd_le32(image + off);
}
static uint16_t img_u16(const uint8_t *image, uint32_t image_size, uint32_t off, int *ok)
{
    if (!image_range_ok(off, 2u, image_size)) { *ok = 0; return 0; }
    return rd_le16(image + off);
}

int ee_elf_load(ee_state_t *st, const uint8_t *image, uint32_t image_size,
                 ee_elf_load_result_t *out, const char **err_out)
{
    const char *dummy_err;
    if (!err_out) err_out = &dummy_err;
    *err_out = NULL;
    if (!st || !image || !out) { *err_out = "invalid EE ELF loader argument"; return -1; }
    memset(out, 0, sizeof(*out));

    if (image_size < 52) { *err_out = "image too small for an ELF header"; return -1; }
    if (!(image[0] == 0x7F && image[1] == 'E' && image[2] == 'L' && image[3] == 'F')) {
        *err_out = "bad ELF magic";
        return -1;
    }
    if (image[4] != 1 || image[5] != 1) { *err_out = "not ELF32 little-endian"; return -1; }

    int ok = 1;
    uint16_t e_type      = img_u16(image, image_size, 16, &ok);
    uint16_t e_machine   = img_u16(image, image_size, 18, &ok);
    uint32_t e_entry     = img_u32(image, image_size, 24, &ok);
    uint32_t e_phoff     = img_u32(image, image_size, 28, &ok);
    uint16_t e_phentsize = img_u16(image, image_size, 42, &ok);
    uint16_t e_phnum     = img_u16(image, image_size, 44, &ok);
    if (!ok) { *err_out = "truncated ELF header"; return -1; }
    if (e_machine != 8) { *err_out = "not a MIPS ELF"; return -1; }
    (void)e_type;

    /* ELF32 program headers are 32 bytes. Accept larger entries (extensions)
     * but never walk entries too small for the fields consumed below. */
    if (e_phnum && e_phentsize < 32u) { *err_out = "ELF program header entry too small"; return -1; }
    if (e_phnum) {
        uint64_t table_end = (uint64_t)e_phoff + (uint64_t)e_phentsize * e_phnum;
        if (table_end > image_size) { *err_out = "ELF program header table exceeds image size"; return -1; }
    }

    uint32_t load_start = 0xFFFFFFFFu;
    uint32_t load_end = 0;
    int any_load = 0;

    for (uint16_t i = 0; i < e_phnum; i++) {
        uint64_t ph64 = (uint64_t)e_phoff + (uint64_t)i * e_phentsize;
        if (ph64 > 0xFFFFFFFFu) { *err_out = "ELF program header offset overflow"; return -1; }
        uint32_t ph = (uint32_t)ph64;
        uint32_t p_type   = img_u32(image, image_size, ph + 0, &ok);
        uint32_t p_offset = img_u32(image, image_size, ph + 4, &ok);
        uint32_t p_vaddr  = img_u32(image, image_size, ph + 8, &ok);
        uint32_t p_filesz = img_u32(image, image_size, ph + 16, &ok);
        uint32_t p_memsz  = img_u32(image, image_size, ph + 20, &ok);
        if (!ok) { *err_out = "truncated program header"; return -1; }
        if (p_type != 1u) continue;

        if (p_filesz > p_memsz) { *err_out = "PT_LOAD file size exceeds memory size"; return -1; }
        if (!image_range_ok(p_offset, p_filesz, image_size)) { *err_out = "PT_LOAD segment exceeds image size"; return -1; }

        uint64_t segment_end64 = (uint64_t)p_vaddr + p_memsz;
        if (segment_end64 > st->ram_size || segment_end64 > 0xFFFFFFFFu) {
            *err_out = "PT_LOAD segment exceeds EE RAM";
            return -1;
        }
        uint32_t segment_end = (uint32_t)segment_end64;

        for (uint32_t b = 0; b < p_filesz; b++)
            ee_mem_write8(st, p_vaddr + b, image[p_offset + b]);
        for (uint32_t b = p_filesz; b < p_memsz; b++)
            ee_mem_write8(st, p_vaddr + b, 0);

        if (p_vaddr < load_start) load_start = p_vaddr;
        if (segment_end > load_end) load_end = segment_end;
        any_load = 1;
    }
    if (!any_load) { *err_out = "no PT_LOAD segments found"; return -1; }

    out->entry = e_entry;
    out->load_start = load_start;
    out->load_end = load_end;
    return 0;
}
