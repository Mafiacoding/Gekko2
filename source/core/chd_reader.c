#include "core/chd_reader.h"
#include "libchdr/chd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct gekko2_chd {chd_file *file;uint8_t *hunk;uint32_t hunk_bytes,cached,unit,sectors,offset;};
static uint64_t stats[4];
uint64_t gekko2_chd_stat(unsigned n){return n<4?stats[n]:0;}
void gekko2_chd_close(struct gekko2_chd *d){if(!d)return;if(d->file)chd_close(d->file);free(d->hunk);free(d);}
static int read_bytes(struct gekko2_chd *d,uint64_t pos,uint8_t *out,size_t size){
 while(size){uint64_t h=pos/d->hunk_bytes;unsigned off=pos%d->hunk_bytes;
  if(h>UINT32_MAX)return -1;
  if(d->cached!=(uint32_t)h){d->cached=UINT32_MAX;if(chd_read(d->file,(uint32_t)h,d->hunk)!=CHDERR_NONE){stats[3]++;return -1;}d->cached=(uint32_t)h;stats[1]++;}else stats[2]++;
  size_t n=d->hunk_bytes-off;if(n>size)n=size;memcpy(out,d->hunk+off,n);out+=n;size-=n;pos+=n;
 }return 0;
}
int gekko2_chd_read(struct gekko2_chd *d,uint32_t lba,uint8_t *out){if(!d||!out||lba>=d->sectors)return -1;stats[0]++;return read_bytes(d,(uint64_t)lba*d->unit+d->offset,out,2048);}
int gekko2_chd_open(const char *path,struct gekko2_chd **out,uint32_t *sectors){
 if(!path||!out||!sectors)return -1;*out=NULL;
 struct gekko2_chd *d=calloc(1,sizeof(*d));if(!d)return -1;d->cached=UINT32_MAX;
 if(chd_open(path,CHD_OPEN_READ,NULL,&d->file)!=CHDERR_NONE)goto fail;
 const chd_header *h=chd_get_header(d->file);
 /* Bound per-image decode memory. Unsupported parents/codecs fail in libchdr. */
 if(!h||!h->hunkbytes||h->hunkbytes>1024u*1024u||!h->unitbytes||!h->logicalbytes||h->logicalbytes%h->unitbytes)goto fail;
 d->hunk_bytes=h->hunkbytes;d->unit=h->unitbytes;
 uint64_t count=h->logicalbytes/d->unit;if(count>UINT32_MAX)goto fail;d->sectors=(uint32_t)count;
 if(d->unit!=2048){
  if(d->unit!=2448)goto fail;
  char meta[256]={0},type[64]={0},sub[64]={0},pgtype[64]={0},pgsub[64]={0};uint32_t len;
  int track=0,frames=0,pregap=0,postgap=0;
  chd_error e=chd_get_metadata(d->file,CDROM_TRACK_METADATA2_TAG,0,meta,sizeof(meta)-1,&len,NULL,NULL);
  if(e==CHDERR_NONE){if(sscanf(meta,"TRACK:%d TYPE:%63s SUBTYPE:%63s FRAMES:%d PREGAP:%d PGTYPE:%63s PGSUB:%63s POSTGAP:%d",&track,type,sub,&frames,&pregap,pgtype,pgsub,&postgap)!=8)goto fail;}
  else {if(chd_get_metadata(d->file,CDROM_TRACK_METADATA_TAG,0,meta,sizeof(meta)-1,&len,NULL,NULL)!=CHDERR_NONE||sscanf(meta,"TRACK:%d TYPE:%63s SUBTYPE:%63s FRAMES:%d",&track,type,sub,&frames)!=4)goto fail;}
  /* Multi-track/audio/pregap layouts require a real track map, never guess. */
  if(track!=1||frames<=16||pregap||postgap||!strcmp(type,"AUDIO")||(uint32_t)frames>d->sectors)goto fail;
  if(chd_get_metadata(d->file,CDROM_TRACK_METADATA2_TAG,1,meta,sizeof(meta)-1,&len,NULL,NULL)==CHDERR_NONE||chd_get_metadata(d->file,CDROM_TRACK_METADATA_TAG,1,meta,sizeof(meta)-1,&len,NULL,NULL)==CHDERR_NONE)goto fail;
  d->sectors=(uint32_t)frames;
 }
 d->hunk=malloc(d->hunk_bytes);if(!d->hunk)goto fail;
 uint8_t pvd[2048];unsigned offsets[3]={0,16,24};int found=0;
 for(unsigned i=0;i<(d->unit==2048?1u:3u);i++){d->offset=offsets[i];if(!gekko2_chd_read(d,16,pvd)&&pvd[0]==1&&!memcmp(pvd+1,"CD001",5)&&pvd[6]==1){found=1;break;}}
 if(!found)goto fail;*sectors=d->sectors;*out=d;return 0;
fail:gekko2_chd_close(d);return -1;
}
