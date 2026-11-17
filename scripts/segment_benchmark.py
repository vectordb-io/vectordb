#!/usr/bin/env python3
"""Record reproducible native M1 baselines; build the release example first."""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--rows", type=int, nargs="+", default=[10000, 100000])
parser.add_argument("--dimension", type=int, default=128)
parser.add_argument("--seed", type=int, default=24301)
parser.add_argument("--updates", type=int, default=1000)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
binary = ROOT / "target/release/examples/segment_bench"
def command(*args):
    return subprocess.check_output(args, text=True, cwd=ROOT).strip()
report = {
    "format": 1,
    "created_at": datetime.now(timezone.utc).isoformat(),
    "hardware": platform.processor(),
    "platform": platform.platform(),
    "rustc": command("rustc", "--version"),
    "base_commit": command("git", "rev-parse", "HEAD"),
    "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
    "source_sha256": hashlib.sha256(b"".join(p.read_bytes() for p in sorted((ROOT / "crates/engine/src").glob("*.rs")))).hexdigest(),
    "runs": [],
}
if platform.system() == "Darwin":
    report["hardware"] = command("sysctl", "-n", "machdep.cpu.brand_string")
    report["memory_bytes"] = int(command("sysctl", "-n", "hw.memsize"))
for rows in args.rows:
    timer_flag = "-l" if platform.system() == "Darwin" else "-v"
    result = subprocess.run(["/usr/bin/time", timer_flag, str(binary), str(rows), str(args.dimension), str(args.seed), str(args.updates)], check=True, text=True, capture_output=True, timeout=300)
    run = json.loads(result.stdout)
    if platform.system() == "Darwin":
        match = re.search(r"(\d+)\s+maximum resident set size", result.stderr)
        run["peak_rss_bytes"] = int(match.group(1)) if match else None
    else:
        match = re.search(r"Maximum resident set size \(kbytes\): (\d+)", result.stderr)
        run["peak_rss_bytes"] = int(match.group(1)) * 1024 if match else None
    report["runs"].append(run)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
