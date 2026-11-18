#!/usr/bin/env python3
"""Paired tiny-redis/redis ratios and run checks for bench.sh result dirs.

    scripts/bench_report.py <results-dir> [<results-dir>...] [--min-seconds 10] > compare.csv

One row per (run, command, pipeline): medians/min/max per server, ratio of medians, and
per-trial paired ratios (tiny_t / redis_t, same trial number, so both saw the same drift
window). With several dirs, an ALL row per (command, pipeline) gives the ratio_of_medians
range across runs and the paired min-max across all trials.

Warnings on stderr (exit 0): runs under --min-seconds, server CPU < 95%, client CPU > 300%.
Exit 2 on missing raw files or unequal trial counts between servers.
"""
import argparse
import csv
import pathlib
import re
import statistics
import sys

SERVERS = ("tiny-redis", "redis")
EXTS = ("csv", "cpu", "clientcpu")
NAME = re.compile(r"^(tiny-redis|redis)-(set|get)-P(\d+)-t(\d+)\.(csv|cpu|clientcpu)$")
COLUMNS = ["run", "command", "pipeline", "trials",
           "tiny_rps_median", "tiny_rps_min", "tiny_rps_max",
           "redis_rps_median", "redis_rps_min", "redis_rps_max",
           "ratio_of_medians", "paired_ratio_median", "paired_ratio_min", "paired_ratio_max",
           "tiny_p99_ms_median", "redis_p99_ms_median"]


def requests(p):
    # Mirrors requests() in scripts/bench.sh; keep in sync if that changes.
    return 3_000_000 if p == 1 else 12_000_000


def fail(msg):
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(2)


def load(results, min_seconds):
    raw = pathlib.Path(results) / "raw"
    found = {}  # (server, cmd, p, t) -> set of exts
    for f in raw.iterdir() if raw.is_dir() else ():
        m = NAME.match(f.name)
        if m:
            s, c, p, t, ext = m.groups()
            found.setdefault((s, c, int(p), int(t)), set()).add(ext)
    if not found:
        fail(f"no bench files in {raw}")

    trials = {s: sorted({k[3] for k in found if k[0] == s}) for s in SERVERS}
    if trials["tiny-redis"] != trials["redis"]:
        fail(f"{results}: unequal trials: tiny-redis {trials['tiny-redis']}, redis {trials['redis']}")
    trials = trials["redis"]
    combos = sorted({(k[1], k[2]) for k in found}, key=lambda cp: (cp[0] != "set", cp[1]))
    for s in SERVERS:
        for c, p in combos:
            for t in trials:
                missing = set(EXTS) - found.get((s, c, p, t), set())
                if missing:
                    fail(f"{results}: missing {', '.join(f'{s}-{c}-P{p}-t{t}.{e}' for e in sorted(missing))}")

    data = {}  # (cmd, p) -> server -> list of (rps, p99) by trial
    for c, p in combos:
        data[(c, p)] = {}
        for s in SERVERS:
            vals = data[(c, p)][s] = []
            for t in trials:
                name = f"{s}-{c}-P{p}-t{t}"
                with open(raw / f"{name}.csv") as f:
                    rec = list(csv.reader(f))[-1]
                # redis-benchmark --csv columns: test, rps, avg, min, p50, p95, p99, max.
                rps, p99 = float(rec[1]), float(rec[6])
                vals.append((rps, p99))
                secs = requests(p) / rps
                if secs < min_seconds:
                    warn(results, name, f"ran {secs:.2f} s < {min_seconds:g} s")
                cpu = int((raw / f"{name}.cpu").read_text())
                if cpu < 95:
                    warn(results, name, f"server cpu {cpu}% < 95% (not server-bound)")
                ccpu = int((raw / f"{name}.clientcpu").read_text())
                if ccpu > 300:
                    warn(results, name, f"client cpu {ccpu}% > 300% of 400% (client may be the bottleneck)")
    return trials, data


def warn(results, name, msg):
    print(f"warning: {pathlib.Path(results).name} {name}: {msg}", file=sys.stderr)


def report(results, min_seconds):
    trials, data = load(results, min_seconds)
    run = pathlib.Path(results).resolve().name
    rows = []
    for (c, p), d in data.items():
        tiny = [v[0] for v in d["tiny-redis"]]
        red = [v[0] for v in d["redis"]]
        paired = [a / b for a, b in zip(tiny, red)]
        rows.append({
            "run": run, "command": c.upper(), "pipeline": p, "trials": len(trials),
            # round() as in bench.sh's summary, so medians match summary.csv exactly.
            "tiny_rps_median": round(statistics.median(tiny)), "tiny_rps_min": round(min(tiny)), "tiny_rps_max": round(max(tiny)),
            "redis_rps_median": round(statistics.median(red)), "redis_rps_min": round(min(red)), "redis_rps_max": round(max(red)),
            "ratio_of_medians": f"{statistics.median(tiny) / statistics.median(red):.3f}",
            "paired_ratio_median": f"{statistics.median(paired):.3f}",
            "paired_ratio_min": f"{min(paired):.3f}", "paired_ratio_max": f"{max(paired):.3f}",
            "tiny_p99_ms_median": round(statistics.median(v[1] for v in d["tiny-redis"]), 3),
            "redis_p99_ms_median": round(statistics.median(v[1] for v in d["redis"]), 3),
        })
    return rows


def combined(rows):
    out = []
    for key in dict.fromkeys((r["command"], r["pipeline"]) for r in rows):
        group = [r for r in rows if (r["command"], r["pipeline"]) == key]
        ratios = [r["ratio_of_medians"] for r in group]
        out.append({
            "run": "ALL", "command": key[0], "pipeline": key[1],
            "trials": sum(r["trials"] for r in group),
            "ratio_of_medians": f"{min(ratios, key=float)}-{max(ratios, key=float)}",
            "paired_ratio_min": min((r["paired_ratio_min"] for r in group), key=float),
            "paired_ratio_max": max((r["paired_ratio_max"] for r in group), key=float),
        })
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dirs", nargs="+", metavar="results-dir")
    ap.add_argument("--min-seconds", type=float, default=10)
    args = ap.parse_args()
    rows = [r for d in args.dirs for r in report(d, args.min_seconds)]
    if len(args.dirs) > 1:
        rows += combined(rows)
    w = csv.DictWriter(sys.stdout, COLUMNS, lineterminator="\n")
    w.writeheader()
    w.writerows(rows)


if __name__ == "__main__":
    main()
