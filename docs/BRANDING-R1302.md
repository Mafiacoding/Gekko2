# Gekko2 R1302 artwork

Two original Gekko2 logo assets were created with the built-in image-generation tool. The title uses PS2-inspired angular blue/cyan typography. The larger logo adds an emerald/teal gecko wrapped around the numeral 2. Generation produced 2048x768 masters. Mechanical resizing prepares the HBC 128x48 RGB PNG and a 256x96 RGBA launcher header. The original generated masters are preserved.

## Asset locations

- assets/branding/hbc-wordmark-master.png: original full-resolution wordmark.
- assets/branding/icon.png: copy to sd:/apps/gekko2/icon.png next to boot.dol and meta.xml.
- assets/branding/menu-gecko-master.png: original full-resolution transparent mascot wordmark.
- assets/branding/menu-gecko.png: transparent runtime header.
- include/core/hw/frontend_logo_data.h: embedded 256x96 RGBA words for the runtime header.
- source/hw/frontend_logo.c: clipped logical-640x480 launcher-only alpha composition into packed YCbCr XFB.

The launcher uses the embedded header with alpha preserved; it does not decode PNG or load menu art from SD at runtime. Browser/BIOS controls, EE/IOP JIT and GX guest rendering are unchanged from R1301. Native menu preview is a host render of the same launcher routines, not a screenshot from real Wii hardware.

## Final generation prompts

HBC icon: finished wide 8:3 Nintendo Wii Homebrew Channel logo; exact GEKKO2 lettering; custom geometric continuous-line typography inspired by PlayStation 2; large electric cobalt-blue to icy cyan strokes, midnight navy backdrop, restrained halo, crisp edges and safe margins, readable at 128x48; no other words, mascot, mockup or watermark.

Main-menu logo: transparent wide 8:3 header; exact GEKKO2 spelling; readable angular blue/cyan continuous-line wordmark; polished three-dimensional emerald/teal gecko with cobalt accents and pale adhesive toe pads climbs around the numeral 2, long curled tail underlines lettering; preserve readability, all artwork inside margins, restrained edge glow; no surrounding UI, background rectangle, slogan or watermark. Built-in image-generation mode was used, not a CLI/API fallback.

## Verification

HBC PNG dimensions and RGB format were checked. The header has a real alpha channel. The complete menu was rendered through the native launcher/XFB composition routines and inspected at 640x480. Both devkitPPC builds are included. Guest runtime logic is unchanged; no new hardware timing or BIOS boot claim is made.
