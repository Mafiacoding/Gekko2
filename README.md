# Crash GX source and toolchain handoff

Development snapshot, not a verified gameplay release. No private game data or game DOL included.

Download this branch using Code > Download ZIP, then extract it. GitHub limits individual Git files to 100 MB, so the 890,159,045-byte toolchain ZIP is split into 48 MiB pieces. All pieces are needed.

## Windows
Open Command Prompt (cmd.exe) in the extracted directory and run:

```bat
copy /b Crash-GX-Complete-With-Toolchains.zip.part001+Crash-GX-Complete-With-Toolchains.zip.part002+Crash-GX-Complete-With-Toolchains.zip.part003+Crash-GX-Complete-With-Toolchains.zip.part004+Crash-GX-Complete-With-Toolchains.zip.part005+Crash-GX-Complete-With-Toolchains.zip.part006+Crash-GX-Complete-With-Toolchains.zip.part007+Crash-GX-Complete-With-Toolchains.zip.part008+Crash-GX-Complete-With-Toolchains.zip.part009+Crash-GX-Complete-With-Toolchains.zip.part010+Crash-GX-Complete-With-Toolchains.zip.part011+Crash-GX-Complete-With-Toolchains.zip.part012+Crash-GX-Complete-With-Toolchains.zip.part013+Crash-GX-Complete-With-Toolchains.zip.part014+Crash-GX-Complete-With-Toolchains.zip.part015+Crash-GX-Complete-With-Toolchains.zip.part016+Crash-GX-Complete-With-Toolchains.zip.part017+Crash-GX-Complete-With-Toolchains.zip.part018 Crash-GX-Complete-With-Toolchains.zip
```

## Linux / WSL

```sh
cat Crash-GX-Complete-With-Toolchains.zip.part* > Crash-GX-Complete-With-Toolchains.zip
sha256sum Crash-GX-Complete-With-Toolchains.zip
```

Expected SHA-256: `e3eef7fa28907d860f31aa5904096787a085724aff64a08189528ee3d75c33c6`.

Then extract the joined ZIP and read TOOLCHAINS-README.txt. Toolchains target Linux x86_64 / WSL2. Alternatively, the 14 MB source-only ZIP can be downloaded independently.
