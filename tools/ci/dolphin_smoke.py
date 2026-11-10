#!/usr/bin/env python3
"""Private-runner Dolphin smoke test: no BIOS, discs or raw guest logs uploaded."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
out = root / "outputs" / "dolphin-ci"
out.mkdir(parents=True, exist_ok=True)
summary = {"status": "not_started", "seconds": 0}
start = time.monotonic()
try:
    base_raw = os.environ.get("GEKKO2_TEST_DATA", "")
    if not base_raw:
        raise ValueError("Set GEKKO2_TEST_DATA as a runner/repository variable to a private test data directory")
    base = Path(base_raw).expanduser().resolve(strict=True)
    filename = os.environ.get("TEST_IMAGE", "")
    if not filename or Path(filename).is_absolute():
        raise ValueError("TEST_IMAGE must be a relative path within the private test directory")
    target = (base / filename).resolve(strict=True)
    if not target.is_relative_to(base) or not target.is_file():
        raise ValueError("TEST_IMAGE is not a regular file inside GEKKO2_TEST_DATA")
    max_seconds = int(os.environ.get("MAX_SECONDS", "180"))
    if not 1 <= max_seconds <= 1200:
        raise ValueError("MAX_SECONDS must be from 1 to 1200")
    dolphin = os.environ.get("DOLPHIN_BIN", "dolphin-emu")
    profile = os.environ.get("GEKKO2_DOLPHIN_PROFILE", str(base / "dolphin-profile"))
    cmd = [dolphin, "--batch", "--user", profile, "--exec", str(target)]
    try:
        result = subprocess.run(cmd, timeout=max_seconds, stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL, check=False)
        summary["status"] = "exited"
        summary["exit_code"] = result.returncode
    except subprocess.TimeoutExpired:
        summary["status"] = "timeout"
        # A timeout is expected for long-running BIOS smoke tests, not a boot-success assertion.
except Exception as exc:
    summary["status"] = "runner_error"
    summary["error_type"] = type(exc).__name__
finally:
    summary["seconds"] = round(time.monotonic() - start, 2)
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary))
sys.exit(1 if summary["status"] == "runner_error" else 0)
