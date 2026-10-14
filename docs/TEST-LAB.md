# GEKKO2 test lab: cloud CI + private Dolphin runner

## What is automated
- On every PR and push to main, GitHub-hosted Ubuntu runs `tools/verify_regressions.py` and uploads the native test results.
- A separate manually triggered workflow can run Dolphin on your private **self-hosted Linux x64** machine, which can be your PC or a rented Linux VM with graphics/display support.
- GitHub hosted CI does **not** emulate Wii hardware or establish PS2 game compatibility. The Dolphin workflow is only a smoke-runner; timeout does not mean success.

## Private Dolphin runner setup
1. Install Dolphin on the Linux runner and confirm `dolphin-emu --help` provides `--batch`, `--user`, and `--exec`. Provide virtual display/GPU support if headless.
2. In GitHub repository Settings > Actions > Runners, register a self-hosted runner with the label `gekko2-dolphin` (plus Linux/x64 labels).
3. Store your legally obtained BIOS, game images, and test homebrew **only on that machine**, in a private filesystem directory. Never place them in the repository or in GitHub artifacts.
4. Set GitHub Actions repository variable `GEKKO2_TEST_DATA` to that directory. Optionally set `DOLPHIN_BIN` and `GEKKO2_DOLPHIN_PROFILE` on the runner.
5. Start Actions > Gekko2 private Dolphin runner > Run workflow. Supply a relative filename within the test data directory.
6. Inspect artifact `dolphin-smoke-summary` (only exit code/elapsed time/status). BIOS/game logs remain private on the runner.

**Security:** Running untrusted pull requests on a self-hosted runner is unsafe. This Dolphin workflow has manual triggering only, read-only repository permissions, and no BIOS/game contents in its outputs. Restrict repository write access, runner access and local file permissions. Do not use a runner with unrelated credentials.

## Next integration stage
Wire the project's own debug-counter exports and sanitized per-frame statistics into the runner, then implement deterministic checkpoints for SCE, OSDSYS, Tekken character selection, GT3 copyright screen and GX screenshot comparisons. Those are **not currently validated** by this basic runner.

## Source checkpoint
The repository README at the time of setup describes R1308. Newer BETA12 sources must be compared and verified before replacing main.
