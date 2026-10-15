/* iso_loader.c - ISO9660 game-disc loader. */
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#include "core/iso_loader.h"
#include "core/chd_reader.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#define ISO_CACHE_SECTORS 16u
struct iso_sector_cache {
    uint64_t image_size;
    uint32_t first, count;
    uint8_t raw[ISO_CACHE_SECTORS * ISO_RAW_SECTOR_SIZE];
};
static uint64_t cache_stats[5];
uint64_t iso_cache_stat(unsigned n){return n<5u?cache_stats[n]:0;}

static uint32_t read_le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

static int seek64(FILE *fp,uint64_t pos)
{
    off_t off=(off_t)pos;
    if(off<0||(uint64_t)off!=pos)return -1;
    return fseeko(fp,off,SEEK_SET)==0?0:-1;
}

static int file_size(FILE *fp,uint64_t *out)
{
    if(!fp||!out)return -1;
    off_t cur=ftello(fp);if(cur<0)return -1;
    if(fseeko(fp,0,SEEK_END)!=0)return -1;
    off_t end=ftello(fp);if(end<0){fseeko(fp,cur,SEEK_SET);return -1;}
    if(fseeko(fp,cur,SEEK_SET)!=0)return -1;
    *out=(uint64_t)end;return 0;
}

static int read_sector_raw(FILE *fp,uint32_t lba,uint32_t stride,uint32_t data_offset,uint8_t *buf)
{
    if(!fp||!buf||stride<ISO_SECTOR_SIZE||data_offset>stride||ISO_SECTOR_SIZE>stride-data_offset)return -1;
    uint64_t phys=(uint64_t)lba*stride+data_offset;
    uint64_t end=phys+ISO_SECTOR_SIZE,size=0;
    if(end<phys||file_size(fp,&size)!=0||end>size)return -1;
    if(seek64(fp,phys)!=0)return -1;
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
    uint8_t signature[8];size_t got=fread(signature,1,8,fp);rewind(fp);
    if(got==8&&!memcmp(signature,"MComprHD",8)) {
        fclose(fp);uint32_t sectors;struct gekko2_chd *chd=NULL;uint8_t pvd[ISO_SECTOR_SIZE];
        if(gekko2_chd_open(path,&chd,&sectors)||gekko2_chd_read(chd,16,pvd)) {gekko2_chd_close(chd);return -1;}
        const uint8_t *r=pvd+156;uint32_t lba=read_le32(r+2),bytes=read_le32(r+10);
        if(r[0]<34||r[32]!=1||r[33]!=0||!(r[25]&2)||!bytes||(uint64_t)lba+((uint64_t)bytes+2047)/2048>sectors) {gekko2_chd_close(chd);return -1;}
        out->chd=chd;out->root_lba=lba;out->root_size=bytes;out->physical_stride=2048;out->opened=1;return 0;
    }
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
    out->root_lba=root_lba;out->root_size=root_size;out->physical_stride=stride;out->data_offset=data_offset;out->fp=fp;out->opened=1;
    /* Allocation failure keeps the checked original reader available. */
    out->sector_cache=calloc(1,sizeof(*out->sector_cache));
    if(out->sector_cache)out->sector_cache->image_size=size;
    return 0;
}

void iso_close(iso_image_t *img)
{
    if(!img)return;
    if(img->opened&&img->fp)fclose(img->fp);
    gekko2_chd_close(img->chd);
    free(img->sector_cache);memset(img,0,sizeof(*img));
}
int iso_read_sector(iso_image_t *img,uint32_t lba,uint8_t *buf)
{
    if(!img||!img->opened||!buf)return -1;
    if(img->chd)return gekko2_chd_read(img->chd,lba,buf);
    if(!img->fp)return -1;
    cache_stats[0]++;
    uint32_t stride=img->physical_stride?img->physical_stride:ISO_SECTOR_SIZE;
    struct iso_sector_cache *c=img->sector_cache;
    if(!c)return read_sector_raw(img->fp,lba,stride,img->data_offset,buf);
    if(stride<ISO_SECTOR_SIZE||stride>ISO_RAW_SECTOR_SIZE||
       img->data_offset>stride||ISO_SECTOR_SIZE>stride-img->data_offset)return -1;
    if(c->count && lba>=c->first && lba-c->first<c->count){
        memcpy(buf,c->raw+(lba-c->first)*stride,ISO_SECTOR_SIZE);cache_stats[1]++;return 0;
    }
    uint64_t requested=(uint64_t)lba*stride+img->data_offset;
    if(requested+ISO_SECTOR_SIZE>c->image_size){cache_stats[4]++;return -1;}
    uint32_t first=lba&~(ISO_CACHE_SECTORS-1u);
    uint64_t base=(uint64_t)first*stride+img->data_offset;
    uint64_t available=(c->image_size-base-ISO_SECTOR_SIZE)/stride+1u;
    uint32_t count=available<ISO_CACHE_SECTORS?(uint32_t)available:ISO_CACHE_SECTORS;
    size_t bytes=(size_t)(count-1u)*stride+ISO_SECTOR_SIZE;
    c->count=0;
    if(seek64(img->fp,base)||fread(c->raw,1,bytes,img->fp)!=bytes){
        cache_stats[4]++;
        /* A short/truncated read never publishes partially initialized data. */
        return read_sector_raw(img->fp,lba,stride,img->data_offset,buf);
    }
    c->first=first;c->count=count;cache_stats[2]++;cache_stats[3]+=bytes;
    memcpy(buf,c->raw+(lba-first)*stride,ISO_SECTOR_SIZE);return 0;
}

/* ISO9660 level-1 file identifiers are conventionally uppercase. A PS2
 * cdrom0: open with lower-case path components must resolve the same real
 * directory entries; do not modify the requested file or its payload. */
static int iso9660_name_equals(const char *a,const char *b)
{
    while(*a && *b) {
        unsigned char x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='a'&&x<='z')x=(unsigned char)(x-('a'-'A'));
        if(y>='a'&&y<='z')y=(unsigned char)(y-('a'-'A'));
        if(x!=y)return 0;
    }
    return *a==*b;
}
int iso_find_in_root(iso_image_t *img,const char *name,iso_dirent_t *out)
{
    if(!img||!img->opened||!name||!out)return -1;
    uint32_t remaining=img->root_size,lba=img->root_lba;uint8_t sector[ISO_SECTOR_SIZE];
    while(remaining){if(iso_read_sector(img,lba,sector)!=0)return -1;uint32_t valid=remaining<ISO_SECTOR_SIZE?remaining:ISO_SECTOR_SIZE,off=0;
        while(off<valid){uint8_t len=sector[off];if(!len)break;if(len<34u||len>valid-off)return -1;const uint8_t *r=sector+off;uint8_t nl=r[32],flags=r[25];
            if(nl>0&&33u+nl<=len){char en[224];uint32_t n=nl<sizeof(en)-1?nl:sizeof(en)-1;memcpy(en,r+33,n);en[n]=0;if(!(nl==1&&(r[33]==0||r[33]==1))&&iso9660_name_equals(en,name)){uint32_t extent=read_le32(r+2),bytes=read_le32(r+10);if(bytes){uint64_t count=((uint64_t)bytes+ISO_SECTOR_SIZE-1)/ISO_SECTOR_SIZE;if(count>UINT32_MAX||(uint64_t)extent+count-1u>UINT32_MAX)return -1;uint8_t probe[ISO_SECTOR_SIZE];if(iso_read_sector(img,extent+(uint32_t)(count-1),probe)!=0)return -1;}out->lba=extent;out->size=bytes;out->is_directory=(flags&2)?1:0;memcpy(out->name,en,n+1);return 0;}}
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
