#include <stdint.h>
#include <stdio.h>
#include "hw/dma.c"
int main(void)
{
    uint8_t ram[64] = {0}, scratch[16384] = {0}, *p = 0;
    dma_bind_ee_ram(ram, sizeof(ram));
    dma_bind_scratchpad(scratch, sizeof(scratch));
    int fail = 0;
    fail += dma_resolve_ptr(63, 2, &p) != 0;
    fail += dma_resolve_ptr(63, UINT32_MAX, &p) != 0;
    fail += dma_resolve_ptr(0x80000001u, UINT32_MAX, &p) != 0;
    fail += dma_resolve_ptr(0x80003fffu, 2, &p) != 0;
    fail += dma_resolve_ptr(63, 1, &p) != 1 || p != ram + 63;
    fail += dma_resolve_ptr(0x80003fffu, 1, &p) != 1 || p != scratch + 16383;
    dma_bind_scratchpad(scratch, 8);
    fail += dma_resolve_ptr(0x80000009u, 0, &p) != 0;
    fail += dma_resolve_ptr(0x80000007u, 1, &p) != 1 || p != scratch + 7;
    printf("%s: 8 DMA bounds checks\n", fail ? "FAIL" : "PASS");
    return !!fail;
}
