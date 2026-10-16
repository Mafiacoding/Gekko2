Add an independent, factory-OFF "GX RISK MODE - FAST TRIANGLES" option to the CPU/GX menu. It saves to sd:/pcsx2/gx-risk.cfg, applies only at Cold Boot and leaves Chain/PSM settings independent. UP from the first optimization selects the new entry; A/Left/Right toggles it, using existing controller mappings.

ON allows one GX triangle submission when all covered GS pixels permit RGB writes and share the same PABE gate. It retains canonical CPU alpha/Z decisions, original unpadded scissor bounds, source/destination admission, alias checks and GX fences. Mixed permissions, SCANMSK and unsupported GS states keep their prior paths. The intentional risk is RGB edge coverage differing between GX and GS, not bypassing safety or unfinished hardware behavior.

Validation: 55 host suites; 131,072 new color/PABE masks; four separate availability/persistence build profiles. Linked PPC tests show 16 submissions with OFF versus 1 with ON in the synthetic eligible case, unchanged alpha/Z metadata, cold-boot request isolation, correct scissor and PABE/ATE/DATE/Z/SCANMSK fallbacks. Existing format, shading/ownership, fence and cold-boot checks pass. Wii DOL/ELF and static MEM1 checks pass. GX is mocked in PPC tests; no real-Wii edge accuracy, FPS or gameplay improvement is claimed.

Stacked on codex/beta12-26-generic-gx. See docs/BETA12.27-GX-RISK.md. No private assets or user logs included.
