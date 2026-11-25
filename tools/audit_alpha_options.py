#!/usr/bin/env python3
"""Inventory the real option implementation without running a Wii emulator.

Compiles optimization.c/experimental_options.c for menu capability profiles.
GEKKO here selects option metadata only: this is not a PPC or hardware test.
Writes generated files outside the checkout and never reads SD/user settings.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROBE = r'''
#include "core/recompiler/optimization.h"
#include "core/recompiler/experimental_options.h"
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    gekko2_opt_apply();
    printf("MASK\t%08x\t%08x\t%08x\n", gekko2_opt_available(),
           gekko2_opt_requested(), gekko2_optimization_mask);
    for (unsigned n = 0; n < GEKKO2_OPT_COUNT; n++)
        printf("OPT\t%u\t%s\t%s\n", n, gekko2_opt_name(n),
               gekko2_opt_description(n));
    if (gekko2_opt_load(argv[1])) return 3;
    gekko2_opt_apply();
    printf("CLEAR\t%08x\t%08x\n", gekko2_opt_requested(),
           gekko2_optimization_mask);
    gekko2_experimental_apply_cold_boot();
    for (unsigned n = 0; n < GEKKO2_EXP_COUNT; n++)
        printf("EXP\t%u\t%u\t%d\t%d\n", n,
               (gekko2_experimental_available() >> n) & 1u,
               gekko2_experimental_requested(n), gekko2_experimental_enabled(n));
    return 0;
}
'''
PROFILES = {
    "host_metadata": [],
    "wii_default_metadata": ["-DGEKKO"],
    "beta12_27_metadata": ["-DGEKKO", "-DGEKKO2_EE_BLOCK_FAST",
        "-DGEKKO2_DSI_FASTMEM", "-DGEKKO2_GX_NATIVE_BIOS_TRI",
        "-DGEKKO2_EE_CHAIN_RESIDENCY", "-DGEKKO2_GX_GENERIC_SHADER",
        "-DGEKKO2_GX_EXTENDED_TRIANGLES", "-DGEKKO2_GX_RISK_MODE"],
    "interpreter_metadata": ["-DGEKKO", "-DPCSX2WII_JIT_DISABLE"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-json", type=Path)
    parser.add_argument("--cc", default="cc", help="C compiler command")
    args = parser.parse_args()
    enum = (ROOT / "include/core/recompiler/optimization.h").read_text()
    keys = re.findall(r"\bGEKKO2_OPT_[A-Z0-9_]+\b",
                      enum.split("enum {", 1)[1].split("};", 1)[0])
    assert keys.pop() == "GEKKO2_OPT_COUNT"
    uses = {key: [] for key in keys}
    for path in sorted((ROOT / "source").rglob("*")):
        if path.suffix not in (".c", ".h", ".inc"):
            continue
        if path.name in ("main.c", "optimization.c"):
            continue
        for number, line in enumerate(path.read_text().splitlines(), 1):
            for key in set(re.findall(r"\bGEKKO2_OPT_[A-Z0-9_]+\b", line)) & uses.keys():
                uses[key].append(f"{path.relative_to(ROOT)}:{number}")
    profiles = {}
    with tempfile.TemporaryDirectory(prefix="gekko2-options-audit-") as tmp:
        directory = Path(tmp)
        source = directory / "probe.c"
        source.write_text(PROBE)
        cleared = directory / "cleared.cfg"
        cleared.write_text("GEKKO2_OPTIONS 9 00000000\n")
        for name, flags in PROFILES.items():
            exe = directory / name
            subprocess.run([*shlex.split(args.cc), "-std=c11", "-O2", "-Iinclude",
                *flags, str(source), "source/core/recompiler/optimization.c",
                "source/core/recompiler/experimental_options.c", "-o", str(exe)],
                cwd=ROOT, check=True)
            lines = subprocess.check_output([str(exe), str(cleared)], text=True).splitlines()
            result = {"metadata_defines": flags, "options": [], "experiments": []}
            for line in lines:
                fields = line.split("\t")
                if fields[0] == "MASK":
                    available, requested, active = [int(v, 16) for v in fields[1:]]
                    result.update(available_mask=f"{available:08x}",
                                  requested_default_mask=f"{requested:08x}",
                                  active_default_mask=f"{active:08x}")
                elif fields[0] == "OPT":
                    index = int(fields[1])
                    result["options"].append(dict(index=index, key=keys[index],
                        name=fields[2], description=fields[3],
                        available=bool(available & (1 << index)),
                        requested_default=bool(requested & (1 << index)),
                        active_default=bool(active & (1 << index)),
                        source_references=uses[keys[index]]))
                elif fields[0] == "CLEAR":
                    result["all_off_requested_mask"] = fields[1]
                    result["all_off_active_mask"] = fields[2]
                elif fields[0] == "EXP":
                    result["experiments"].append(dict(index=int(fields[1]),
                        available=bool(int(fields[2])), requested=bool(int(fields[3])),
                        active=bool(int(fields[4]))))
            assert len(result["options"]) == len(keys)
            profiles[name] = result
            print(f"{name}: {len(keys)} options, requested={requested:08x}, "
                  f"active={active:08x}, all-OFF active={result['all_off_active_mask']}")
    report = dict(schema=1, source_commit=subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        scope="Compiled option metadata, not PPC execution or hardware availability",
        profiles=profiles)
    if args.output_json:
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(json.dumps(report, indent=2) + "\n")
    print("Runtime capability/dependency validation remains separate from available_mask.")


if __name__ == "__main__":
    main()
