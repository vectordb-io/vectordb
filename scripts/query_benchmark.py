#!/usr/bin/env python3
"""Run the fixed-recall M2 workload after building the release query_bench example."""
import hashlib
import json
import platform
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
binary = ROOT / "target/release/examples/query_bench"
args = ["/usr/bin/time", "-l" if platform.system() == "Darwin" else "-v", str(binary), "10000"]
result = subprocess.run(args, check=True, text=True, capture_output=True, cwd=ROOT, timeout=300)
report = json.loads(result.stdout)
report.update({"created_at": datetime.now(timezone.utc).isoformat(), "platform": platform.platform(), "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(), "rustc": subprocess.check_output(["rustc", "--version"], text=True).strip(), "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(), "source_sha256": hashlib.sha256(b"".join(p.read_bytes() for p in sorted((ROOT / "crates/engine/src").glob("*.rs")))).hexdigest()})
if platform.system() == "Darwin":
    report["hardware"] = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
    report["memory_bytes"] = int(subprocess.check_output(["sysctl", "-n", "hw.memsize"], text=True))
    report["peak_rss_bytes"] = int(re.search(r"(\d+)\s+maximum resident set size", result.stderr).group(1))
else:
    report["peak_rss_bytes"] = int(re.search(r"Maximum resident set size \(kbytes\): (\d+)", result.stderr).group(1)) * 1024
path = ROOT / "docs/benchmarks/m2-baseline.json"
path.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"peak_rss_bytes":report["peak_rss_bytes"],"results":[{k:v for k,v in r.items() if k!="status"} for r in report["results"]]},indent=2))
