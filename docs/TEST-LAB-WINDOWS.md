# Free Windows test runner

1. Install Dolphin and Python 3 on your Windows PC.
2. Under GitHub repository **Settings > Actions > Runners > New self-hosted runner**, select Windows x64; follow GitHub's current instructions on your PC. Add label `gekko2-windows`. Run the agent interactively under your Windows account if Dolphin needs your desktop or GPU; do not use it for untrusted jobs.
3. Keep your legally obtained BIOS / games / test DOLs in a private local folder, not in GitHub or artifacts.
4. In repository **Settings > Secrets and variables > Actions > Variables**, create `GEKKO2_TEST_DATA` with the **absolute path** to that folder, e.g. `C:\\Gekko2-private-tests`. Create `DOLPHIN_BIN` with the absolute path to Dolphin.exe.
5. From **Actions > Dolphin private Windows runner > Run workflow**, select the feature branch if not merged, enter an existing **relative file name** and a test duration.
6. Check the small `dolphin-windows-smoke` artifact. Status `timeout` only indicates that Dolphin kept running until the limit, **not** a successful BIOS or game boot.

The Windows runner is a private machine, not a free always-on cloud server. It is available only while your PC and its GitHub runner are running. Do not expose sensitive files or personal accounts to jobs or third-party pull requests.

The separate `Dolphin cloud smoke (free Ubuntu)` workflow installs Dolphin and validates that it can start with a virtual display and software Mesa. It does **not** boot Gekko2 or benchmark GPU FPS. Private gameplay tests require the self-hosted runner.
