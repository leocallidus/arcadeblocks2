#!/usr/bin/env python3
"""Run Arcade Blocks II Phase 1.5 Stage 11 Linux performance baseline."""

from __future__ import annotations

import argparse
import datetime as _dt
import os
import platform
import re
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path


PERF_RE = re.compile(r"PERF_SUMMARY\s+(?P<body>.*)$")
PHASE1_RSS_MIB = {
    "menu": 25.20,
    "level1": 788.61,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--executable",
        type=Path,
        default=Path("build/linux-release/ArcadeBlocksII"),
        help="Path to ArcadeBlocksII executable.",
    )
    parser.add_argument(
        "--assets-dir",
        type=Path,
        default=Path("assets"),
        help="Path to the assets directory.",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=180,
        help="Smoke frames per scenario.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("docs/performance-baseline-linux.md"),
        help="Markdown output path.",
    )
    return parser.parse_args()


def read_rss_kib(pid: int) -> int:
    try:
        with Path(f"/proc/{pid}/status").open("r", encoding="utf-8") as status:
            for line in status:
                if line.startswith("VmRSS:"):
                    parts = line.split()
                    if len(parts) >= 2:
                        return int(parts[1])
    except FileNotFoundError:
        return 0
    return 0


def parse_perf_summary(stderr: str) -> dict[str, str]:
    for line in stderr.splitlines():
        match = PERF_RE.search(line)
        if not match:
            continue
        values: dict[str, str] = {}
        for token in match.group("body").split():
            if "=" not in token:
                continue
            key, value = token.split("=", 1)
            values[key] = value
        return values
    raise RuntimeError("PERF_SUMMARY line was not found in process stderr")


def run_scenario(
    name: str,
    executable: Path,
    assets_dir: Path,
    frames: int,
    extra_args: list[str],
) -> dict[str, object]:
    with tempfile.TemporaryDirectory(prefix=f"arcadeblocks2-{name}-") as data_home:
        env = os.environ.copy()
        env["SDL_VIDEODRIVER"] = "dummy"
        env["SDL_AUDIODRIVER"] = "dummy"
        env["XDG_DATA_HOME"] = data_home

        command = [
            str(executable),
            "--assets-dir",
            str(assets_dir),
            "--windowed",
            "--smoke-frames",
            str(frames),
            "--perf-summary",
            *extra_args,
        ]

        started = time.perf_counter()
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            env=env,
        )

        rss_samples_kib: list[int] = []
        while process.poll() is None:
            rss = read_rss_kib(process.pid)
            if rss > 0:
                rss_samples_kib.append(rss)
            time.sleep(0.005)

        stdout, stderr = process.communicate()
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        if process.returncode != 0:
            sys.stderr.write(stderr)
            raise RuntimeError(f"{name} scenario failed with exit code {process.returncode}")

        perf = parse_perf_summary(stderr)
        peak_rss_kib = max(rss_samples_kib, default=0)
        sample_count = len(rss_samples_kib)
        window = max(1, sample_count // 5)
        early_start = min(sample_count, window)
        early_end = min(sample_count, early_start + window)
        early_samples = rss_samples_kib[early_start:early_end] or rss_samples_kib
        late_samples = rss_samples_kib[-window:] if rss_samples_kib else []
        early_rss_kib = statistics.median(early_samples) if early_samples else 0
        late_rss_kib = statistics.median(late_samples) if late_samples else 0
        return {
            "name": name,
            "command": " ".join(command),
            "elapsed_ms": elapsed_ms,
            "peak_rss_mib": peak_rss_kib / 1024.0,
            "steady_rss_growth_mib": (late_rss_kib - early_rss_kib) / 1024.0,
            "stdout": stdout,
            "stderr": stderr,
            "perf": perf,
        }


def number(values: dict[str, str], key: str) -> float:
    return float(values.get(key, "0"))


def write_report(output: Path, rows: list[dict[str, object]], frames: int) -> None:
    now = _dt.datetime.now(_dt.timezone.utc).astimezone()
    lines = [
        "# Arcade Blocks II Phase 1.5 Stage 11 Linux Performance Baseline",
        "",
        f"Generated: {now.isoformat(timespec='seconds')}",
        "",
        "## Environment",
        "",
        f"- OS: {platform.platform()}",
        f"- Machine: {platform.machine()}",
        f"- Python: {platform.python_version()}",
        f"- Smoke frames per scenario: {frames}",
        "- SDL drivers: `SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`",
        "",
        "## Results",
        "",
        "| Scenario | Startup ms | Avg frame ms | Max frame ms | Peak RSS MiB | vs Phase 1 MiB | Steady growth MiB | Textures | Texture MiB | Load attempts | Cache hits | UI transitions |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]

    for row in rows:
        perf = row["perf"]
        assert isinstance(perf, dict)
        comparison_name = "level1" if row["name"] == "level1" else "menu"
        phase1_rss = PHASE1_RSS_MIB[comparison_name]
        rss_delta = float(row["peak_rss_mib"]) - phase1_rss
        lines.append(
            "| {name} | {startup:.2f} | {avg:.3f} | {max_frame:.3f} | {rss:.2f} | {delta:+.2f} | {growth:+.2f} | {textures} | {texture_mib:.2f} | {loads} | {hits} | {transitions} |".format(
                name=row["name"],
                startup=number(perf, "startup_ms"),
                avg=number(perf, "avg_frame_ms"),
                max_frame=number(perf, "max_frame_ms"),
                rss=float(row["peak_rss_mib"]),
                delta=rss_delta,
                growth=float(row["steady_rss_growth_mib"]),
                textures=perf.get("textures", "0"),
                texture_mib=number(perf, "texture_mib"),
                loads=perf.get("load_attempts", "0"),
                hits=perf.get("cache_hits", "0"),
                transitions=perf.get("ui_transitions", "0"),
            )
        )

    lines.extend(
        [
            "",
            "## Commands",
            "",
        ]
    )
    for row in rows:
        lines.extend([f"- `{row['command']}`"])

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- Startup ms is measured inside the SDL runtime until the selected screen is initialized.",
            "- Process elapsed ms and peak RSS are measured by this Linux tool from outside the process.",
            "- Phase 1 comparison uses menu 25.20 MiB and level 1 788.61 MiB from the 2026-06-08 baseline.",
            "- Settings/help/cycle rows compare against the Phase 1 menu baseline.",
            "- Steady growth compares median RSS after warmup with the final sample window; small allocator/driver variation is expected.",
            "- Texture memory is an RGBA estimate reported by `render::AssetManager`; it is not a driver VRAM query.",
            "- Load attempts count actual image decoder/upload attempts. Cache hits may grow each frame without repeated loading.",
            "- Menu Help intentionally uses text fallback so it does not load the 8192x8192 gameplay atlas.",
            "",
            "## Known Memory Debt",
            "",
            "- Level 1 still loads the legacy 8192x8192 gameplay atlas, estimated at 256 MiB RGBA before renderer overhead.",
            "- Splitting/repacking the gameplay atlas remains Phase 2 work; Stage 11 prevents this debt from affecting menu/settings/help.",
        ]
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    executable = args.executable.resolve()
    assets_dir = args.assets_dir.resolve()

    if not executable.exists():
        raise SystemExit(f"Executable not found: {executable}")
    if not assets_dir.exists():
        raise SystemExit(f"Assets directory not found: {assets_dir}")
    if args.frames <= 0:
        raise SystemExit("--frames must be positive")

    rows = [
        run_scenario("menu", executable, assets_dir, args.frames, ["--smoke-scenario", "main-menu"]),
        run_scenario("settings", executable, assets_dir, args.frames, ["--smoke-scenario", "settings"]),
        run_scenario("help", executable, assets_dir, args.frames, ["--smoke-scenario", "help"]),
        run_scenario("settings-cycle", executable, assets_dir, args.frames, ["--smoke-scenario", "settings-cycle"]),
        run_scenario("help-cycle", executable, assets_dir, args.frames, ["--smoke-scenario", "help-cycle"]),
        run_scenario("level1", executable, assets_dir, args.frames, ["--level", "1"]),
    ]
    write_report(args.output, rows, args.frames)
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
