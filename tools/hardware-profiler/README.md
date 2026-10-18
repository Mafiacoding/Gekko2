# GEKKO2 hardware profiler (experimental)
Standalone Wii homebrew benchmark; no changes to the emulator runtime.
Build with devkitPPC/libogc: `make` in this directory, or download the GitHub Actions artifact.
Run under IOS222 to test the existing /dev/gekko2 GKA1 v1 synchronous RAM-fill protocol.
If the device/capabilities are absent, the ARM test is skipped, not faked.
Results: sd:/gekko2-profiler.csv (USB fallback). Only the IOS worker fill path is measured;
no ARM arithmetic/FPU benchmark, no GX blend/DATE/EFB benchmark, no proof of parallel execution.
This initial standalone implementation has not yet been tested on physical hardware.
