/* Optional BETA12 regression probe for the owner's Crash E3 raw CD image.
 * No game bytes, BIOS or ROM-derived expected payload are bundled.
 * Build: cc -O2 -Iinclude -Isource tools/probe_crash_e3_iso.c -o /tmp/crash_iso_probe
 * Run: /tmp/crash_iso_probe /private/path/to/game.bin
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "core/iso_loader.c"
/* Raw BIN mode does not invoke libchdr. Standalone weak symbols let the
 * probe compile even when the libchdr SDK is not installed. */
__attribute__((weak)) int gekko2_chd_open(const char *p,struct gekko2_chd **d,uint32_t *s)
{(void)p;(void)d;(void)s;return -1;}
__attribute__((weak)) int gekko2_chd_read(struct gekko2_chd *d,uint32_t s,uint8_t *p)
{(void)d;(void)s;(void)p;return -1;}
__attribute__((weak)) void gekko2_chd_close(struct gekko2_chd *d){(void)d;}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|p[1]<<8);}
static int fetch(iso_image_t *img,const char *name,iso_dirent_t *e,uint8_t *head)
{
 if(iso_find_path(img,name,e)||e->is_directory||iso_read_sector(img,e->lba,head)) {
  fprintf(stderr,"FAIL missing/unreadable: %s\n",name);return -1;
 }
 return 0;
}
int main(int argc,char **argv)
{
 if(argc!=2){fprintf(stderr,"Usage: %s /path/to/owner-provided-Crash-E3.bin\n",argv[0]);return 2;}
 iso_image_t img={0};
 if(iso_open(argv[1],&img)){puts("FAIL ISO open");return 1;}
 if(img.physical_stride!=2352u||img.data_offset!=24u){puts("FAIL incorrect raw CD Mode 2 detection");iso_close(&img);return 1;}
 iso_dirent_t cfg,elf,ats,voice;uint8_t buf[2048];
 if(fetch(&img,"cdrom0:/SYSTEM.CNF",&cfg,buf)){iso_close(&img);return 1;}
 char cnf[2049];memcpy(cnf,buf,cfg.size<2048u?cfg.size:2048u);cnf[cfg.size<2048u?cfg.size:2048u]=0;
 if(!strstr(cnf,"BOOT2")||!strstr(cnf,"SCEU_000.01;1")){puts("FAIL boot configuration");iso_close(&img);return 1;}
 if(fetch(&img,"cdrom0:/SCEU_000.01;1",&elf,buf)){iso_close(&img);return 1;}
 if(memcmp(buf,"\177ELF",4)||buf[4]!=1||buf[5]!=1||le16(buf+18)!=8){puts("FAIL boot ELF architecture");iso_close(&img);return 1;}
 uint32_t epc=le32(buf+24),phoff=le32(buf+28);
 uint16_t phentsize=le16(buf+42),phnum=le16(buf+44);
 if(!phnum||phentsize<32||(uint64_t)phoff+(uint64_t)phnum*phentsize>elf.size){puts("FAIL ELF header bounds");iso_close(&img);return 1;}
 uint32_t loaded=0;
 for(uint32_t i=0;i<phnum;i++){
  uint32_t abs=phoff+i*phentsize,sector=abs/2048u,pos=abs%2048u;
  if(pos+32>2048||iso_read_sector(&img,elf.lba+sector,buf))return puts("FAIL ELF program-header read"),1;
  const uint8_t *p=buf+pos;
  if(le32(p)!=1)continue;
  uint32_t offs=le32(p+4),n=le32(p+16),mem=le32(p+20);
  if(n>mem||(uint64_t)offs+n>elf.size)return puts("FAIL ELF PT_LOAD invalid"),1;
  uint32_t final_lba=elf.lba+(offs+n-1u)/2048u;
  if(n && iso_read_sector(&img,final_lba,buf))return puts("FAIL ELF final sector"),1;
  loaded++;
 }
 if(!loaded)return puts("FAIL no loadable PT_LOAD"),1;
 if(fetch(&img,"cdrom0:/ATS/LEVELS/TITLE.ATS;1",&ats,buf)||
    memcmp(buf,"Scriptname",10))return puts("FAIL title script sector"),1;
 if(fetch(&img,"cdrom0:/SFX/VOICE/COR013.VAG;1",&voice,buf))return 1;
 printf("PASS Crash E3 raw Mode2: stride=%u offset=%u, SYSTEM.CNF=%u B, ELF=%u B, entry=0x%08x, PT_LOAD=%u, title ATS=%u B, voice=%u B\n",img.physical_stride,img.data_offset,cfg.size,elf.size,epc,loaded,ats.size,voice.size);
 printf("ISO cache reads=%llu hits=%llu fills=%llu errors=%llu\n",(unsigned long long)iso_cache_stat(0),(unsigned long long)iso_cache_stat(1),(unsigned long long)iso_cache_stat(2),(unsigned long long)iso_cache_stat(4));
 iso_close(&img);return 0;
}
