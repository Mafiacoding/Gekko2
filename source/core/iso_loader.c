/* iso_loader.c - ISO9660 game-disc loader. */
#include "core/iso_loader.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static uint32_t read_le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

static int file_size(FILE *fp,uint64_t *out)
{
    long cur=ftell(fp); if(cur<0)return -1;
    if(fseek(fp,0,SEEK_END)!=0)return -1;
    long end=ftell(fp); if(end<0){fseek(fp,cur,SEEK_SET);return -1;}
    if(fseek(fp,cur,SEEK_SET)!=0)return -1;
    *out=(uint64_t)end;return 0;
}

static int read_sector_raw(FILE *fp,uint32_t lba,uint32_t stride,uint32_t data_offset,uint8_t *buf)
{
    if(!fp||!buf||stride<ISO_SECTOR_SIZE||data_offset>stride||ISO_SECTOR_SIZE>stride-data_offset)return -1;
    uint64_t phys=(uint64_t)lba*stride+data_offset;
    uint64_t end=phys+ISO_SECTOR_SIZE,size=0;
    if(end<phys||file_size(fp,&size)!=0||end>size)return -1;
    if(phys>(uint64_t)LONG_MAX)return -1;
    if(fseek(fp,(long)phys,SEEK_SET)!=0)return -1;
    return fread(buf,1,ISO_SECTOR_SIZE,fp)==ISO_SECTOR_SIZE?0:-1;
}

static int detect_sector_format(FILE *fp,uint32_t *stride,uint32_t *data_offset,uint8_t *pvd)
{
    static const uint32_t c[][2]={{ISO_SECTOR_SIZE,0u},{ISO_RAW_SECTOR_SIZE,ISO_RAW_MODE2_DATA_OFFSET},{ISO_RAW_SECTOR_SIZE,ISO_RAW_MODE1_DATA_OFFSET}};
    for(size_t i=0;i<sizeof(c)/sizeof(c[0]);i++)if(read_sector_raw(fp,ISO_PVD_LBA,c[i][0],c[i][1],pvd)==0&&pvd[0]==1&&memcmp(pvd+1,"CD001",5)==0&&pvd[6]==1){*stride=c[i][0];*data_offset=c[i][1];return 1;}
    return 0;
}

int iso_open(const char *path,iso_image_t *out)
{
    if(!path||!out)return -1;memset(out,0,sizeof(*out));FILE *fp=fopen(path,"rb");if(!fp)return -1;
    uint8_t sector[ISO_SECTOR_SIZE];uint32_t stride=0,data_offset=0;
    if(!detect_sector_format(fp,&stride,&data_offset,sector)){fclose(fp);return -1;}
    const uint8_t *r=sector+156;
    if(r[0]<34u||r[32]!=1u||r[33]!=0u||!(r[25]&0x02u)){fclose(fp);return -1;}
    uint32_t root_lba=read_le32(r+2),root_size=read_le32(r+10);
    if(!root_size){fclose(fp);return -1;}
    uint64_t first=(uint64_t)root_lba*stride+data_offset,size=0;
    uint64_t sectors=((uint64_t)root_size+ISO_SECTOR_SIZE-1u)/ISO_SECTOR_SIZE;
    uint64_t last_end=first+(sectors-1u)*stride+ISO_SECTOR_SIZE;
    if(last_end<first||file_size(fp,&size)!=0||last_end>size){fclose(fp);return -1;}
    out->root_lba=root_lba;out->root_size=root_size;out->physical_stride=stride;out->data_offset=data_offset;out->fp=fp;out->opened=1;return 0;
}

void iso_close(iso_image_t *img){if(img&&img->opened&&img->fp){fclose(img->fp);img->fp=NULL;img->opened=0;}}
int iso_read_sector(iso_image_t *img,uint32_t lba,uint8_t *buf){if(!img||!img->opened||!img->fp||!buf)return -1;uint32_t s=img->physical_stride?img->physical_stride:ISO_SECTOR_SIZE;return read_sector_raw(img->fp,lba,s,img->data_offset,buf);}

int iso_find_in_root(iso_image_t *img,const char *name,iso_dirent_t *out)
{
    if(!img||!img->opened||!name||!out)return -1;
    uint32_t remaining=img->root_size,lba=img->root_lba;uint8_t sector[ISO_SECTOR_SIZE];
    while(remaining){if(iso_read_sector(img,lba,sector)!=0)return -1;uint32_t valid=remaining<ISO_SECTOR_SIZE?remaining:ISO_SECTOR_SIZE,off=0;
        while(off<valid){uint8_t len=sector[off];if(!len)break;if(len<34u||len>valid-off)return -1;const uint8_t *r=sector+off;uint8_t nl=r[32],flags=r[25];
            if(nl>0&&33u+nl<=len){char en[224];uint32_t n=nl<sizeof(en)-1?nl:sizeof(en)-1;memcpy(en,r+33,n);en[n]=0;if(!(nl==1&&(r[33]==0||r[33]==1))&&!strcmp(en,name)){uint32_t extent=read_le32(r+2),bytes=read_le32(r+10);if(bytes){uint64_t count=((uint64_t)bytes+ISO_SECTOR_SIZE-1)/ISO_SECTOR_SIZE;uint8_t probe[ISO_SECTOR_SIZE];if(count&&iso_read_sector(img,extent+(uint32_t)(count-1),probe)!=0)return -1;}out->lba=extent;out->size=bytes;out->is_directory=(flags&2)?1:0;memcpy(out->name,en,n+1);return 0;}}
            off+=len;}
        if(remaining<=ISO_SECTOR_SIZE)break;remaining-=ISO_SECTOR_SIZE;if(lba==UINT32_MAX)return -1;lba++;}
    return -1;
}

int iso_find_path(iso_image_t *img,const char *path,iso_dirent_t *out)
{
    if(!img||!img->opened||!path||!out)return -1;const char *colon=strchr(path,':');if(colon){size_t n=(size_t)(colon-path);if(!((n==5&&!memcmp(path,"cdrom",5))||(n==6&&!memcmp(path,"cdrom",5)&&(path[5]=='0'||path[5]=='1'))))return -1;path=colon+1;}
    iso_image_t dir=*img;for(unsigned depth=0;depth<32;depth++){while(*path=='/'||*path=='\\')path++;if(!*path)return -1;char component[224];size_t n=0;while(path[n]&&path[n]!='/'&&path[n]!='\\'){if(n>=sizeof(component)-3)return -1;component[n]=path[n];n++;}component[n]=0;path+=n;if(!strcmp(component,".")||!strcmp(component,".."))return -1;int more=*path!=0;iso_dirent_t found;int rc=iso_find_in_root(&dir,component,&found);if(rc&&!more&&!strchr(component,';')){component[n++]=';';component[n++]='1';component[n]=0;rc=iso_find_in_root(&dir,component,&found);}if(rc)return -1;if(!more){*out=found;return 0;}if(!found.is_directory)return -1;dir.root_lba=found.lba;dir.root_size=found.size;}
    return -1;
}
