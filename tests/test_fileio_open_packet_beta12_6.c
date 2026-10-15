/* Synthetic ABI regression; optional private Crash E3 disc check. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/ee/ee_core.c"
#include "core/system.h"
static int failed;
#define CHECK(x,msg) do { if (!(x)) { puts("FAIL " msg); failed++; } } while (0)
static void packet(uint8_t *buf,unsigned offset,const char *path)
{
    memset(buf,0xa7,256);
    /* The old Sony 2110 packet has two preceding argument words. */
    memcpy(buf+offset,path,strlen(path)+1);
}
static void check_layout(unsigned offset,const char *name)
{
    uint8_t buf[256];
    packet(buf,offset,name);
    CHECK(ee_fileio_open_name_offset(buf)==offset,"fixed offset selection");
    CHECK(!strcmp((char *)buf+ee_fileio_open_name_offset(buf),name),"no corrupt filename prefix");
}
int main(int argc,char **argv)
{
    check_layout(0,"rom0:OSDSYS");
    check_layout(4,"cdrom0:\\SYSTEM.CNF;1");
    check_layout(8,"cdrom0:\\FONTS\\ARIEL32.BMP;1");
    check_layout(8,"cdrom0:\\FONTS\\ARIEL32.FNT;1");
    check_layout(12,"mc0:\\BIEXEC-SYSTEM");
    uint8_t unknown[24];memset(unknown,0x7f,sizeof unknown);
    CHECK(ee_fileio_open_name_offset(unknown)==4,"unrecognized packet preserves old fallback");
    if(argc>1) {
        bios_image_t bios={0};bios.data=calloc(1,BIOS_MAX_SIZE);bios.size=BIOS_MAX_SIZE;bios.loaded=1;
        CHECK(bios.data!=NULL,"allocate private test RAM");
        if(bios.data&&!system_init(&bios,&bios)) {
            CHECK(iop_cdvd_mount_iso(argv[1])==0,"mount real E3 CD");
            const char *paths[]={"FONTS\\ARIEL32.FNT;1","FONTS\\ARIEL32.BMP;1"};
            for(unsigned i=0;i<2;i++) {
                uint32_t lba=0,size=0;
                int found=iop_cdvd_disc_find_file(paths[i],&lba,&size);
                CHECK(found && lba!=0 && size!=0,"real E3 font exists as ISO9660 entry");
                if(found) printf("private %s: LBA=%u bytes=%u\n",paths[i],lba,size);
            }
            iop_cdvd_unmount_iso();
        } else failed++;
        free(bios.data);
    }
    printf("BETA12.6 FILEIO aligned OPEN packet: %s\n",failed?"FAIL":"PASS");
    return failed?1:0;
}
