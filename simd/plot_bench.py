# /// script
# requires-python = ">=3.12"
# dependencies = ["matplotlib", "seaborn", "numpy", "pandas"]
# ///
"""
Plot SIMD fill-sub-max benchmark results.

Panels:
  (a) Throughput (GB/s) vs vector size, one line per approach
  (b) Speedup relative to scalar, one line per optimized approach
"""

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from matplotlib import gridspec


def main():
    # --- Style Setup ---
    sns.set_theme(font_scale=1.0, style="whitegrid", font="DejaVu Sans")

    # --- Data ---
    data_path = (
        Path(__file__).resolve().parent.parent / "build" / "bench_results_simd.csv"
    )
    if not data_path.exists():
        data_path = Path("bench_results_simd.csv")
    df = pd.read_csv(data_path).dropna()

    # --- Layout: 1 row x 2 cols ---
    fig = plt.figure(figsize=(14, 6), dpi=150)
    gs = gridspec.GridSpec(1, 2)
    gs.update(
        wspace=0.28, left=0.06, right=0.98, top=0.86, bottom=0.14
    )

    ax1 = plt.subplot(gs[0, 0])
    ax2 = plt.subplot(gs[0, 1])

    # --- Colour / style mapping ---
    palette = {
        "scalar": "#4575b4",
        "unrolled4": "#fc8d59",
        "unrolled8": "#fee090",
        "simd": "#d73027",
        "simd_unroll2": "#33a02c",
        "simd_unroll4": "#6a3d9a",
    }
    markers = {
        "scalar": "o", "unrolled4": "s", "unrolled8": "D",
        "simd": "^", "simd_unroll2": "v", "simd_unroll4": "<",
    }
    styles = {
        "scalar": "-", "unrolled4": "--", "unrolled8": ":",
        "simd": "-.", "simd_unroll2": (0, (3, 1, 1, 1)), "simd_unroll4": (0, (5, 1)),
    }

    sorted_approaches = ["scalar", "unrolled4", "unrolled8", "simd", "simd_unroll2", "simd_unroll4"]
    opt_approaches = ["unrolled4", "unrolled8", "simd", "simd_unroll2", "simd_unroll4"]

    # ---- Panel (a): Throughput ----
    for appr in sorted_approaches:
        sub = df[df["approach"] == appr].sort_values("vec_size")
        ax1.plot(
            sub["vec_size"],
            sub["throughput_gbs"],
            color=palette[appr],
            marker=markers[appr],
            linestyle=styles[appr],
            linewidth=2,
            markersize=8,
            markeredgecolor="white",
            markeredgewidth=0.6,
            label=appr,
            zorder=4,
        )
        for _, row in sub.iterrows():
            ax1.text(
                row["vec_size"],
                row["throughput_gbs"] + 0.6,
                f'{row["throughput_gbs"]:.1f}',
                ha="center",
                va="bottom",
                fontsize=6,
                color="dimgrey",
                weight="semibold",
            )

    ax1.set_xlabel("Vector size (elements)", fontsize=10, color="dimgrey")
    ax1.set_ylabel("Throughput (GB/s)", fontsize=10, color="dimgrey")
    ax1.set_xscale("log", base=2)
    ax1.set_xticks([8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    ax1.set_xticklabels(["8", "16", "32", "64", "128", "256", "512", "1K", "2K", "4K"])
    ax1.set_title(
        r"$\bf{(a)}$  Throughput by vector size",
        loc="left",
        fontsize=12,
        pad=6,
    )
    ax1.legend(
        frameon=True,
        facecolor="white",
        framealpha=0.85,
        edgecolor="lightgrey",
        labelcolor="dimgrey",
        loc="upper left",
        fontsize=7,
        title="Approach",
        title_fontsize=7,
    )

    # ---- Panel (b): Speedup relative to scalar ----
    scalar_gbs = (
        df[df["approach"] == "scalar"][["vec_size", "throughput_gbs"]]
        .rename(columns={"throughput_gbs": "scalar_gbs"})
    )
    speedup = (
        df[df["approach"].isin(opt_approaches)]
        .merge(scalar_gbs, on="vec_size")
        .copy()
    )
    speedup["speedup"] = speedup["throughput_gbs"] / speedup["scalar_gbs"]

    for appr in opt_approaches:
        sub = speedup[speedup["approach"] == appr].sort_values("vec_size")
        ax2.plot(
            sub["vec_size"],
            sub["speedup"],
            color=palette[appr],
            marker=markers[appr],
            linestyle=styles[appr],
            linewidth=2,
            markersize=8,
            markeredgecolor="white",
            markeredgewidth=0.6,
            label=appr,
            zorder=4,
        )
        for _, row in sub.iterrows():
            ax2.text(
                row["vec_size"],
                row["speedup"] + 0.035,
                f'{row["speedup"]:.2f}x',
                ha="center",
                va="bottom",
                fontsize=6,
                color="dimgrey",
                weight="semibold",
            )

    ax2.axhline(y=1.0, color="grey", linewidth=0.7, linestyle="--", zorder=1)
    ax2.set_xlabel("Vector size (elements)", fontsize=10, color="dimgrey")
    ax2.set_ylabel("Speedup vs scalar", fontsize=10, color="dimgrey")
    ax2.set_xscale("log", base=2)
    ax2.set_xticks([8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096])
    ax2.set_xticklabels(["8", "16", "32", "64", "128", "256", "512", "1K", "2K", "4K"])
    ax2.set_title(
        r"$\bf{(b)}$  Speedup relative to plain scalar loop",
        loc="left",
        fontsize=12,
        pad=6,
    )
    ax2.legend(
        frameon=True,
        facecolor="white",
        framealpha=0.85,
        edgecolor="lightgrey",
        labelcolor="dimgrey",
        loc="upper left",
        fontsize=7,
        title="Approach",
        title_fontsize=7,
    )

    # --- Shared styling ---
    for ax in [ax1, ax2]:
        ax.grid(axis="y", alpha=0.5, linewidth=0.7)
        ax.tick_params(
            axis="both", which="both", length=0, labelcolor="dimgrey"
        )
        ax.patch.set_edgecolor("lightgrey")
        ax.patch.set_linewidth(0.8)

    sns.despine(left=True, bottom=True)

    # --- Figure-level insight ---
    best = speedup.loc[speedup["speedup"].idxmax()]
    fig.text(
        0.98,
        0.01,
        f"Best: {best['approach']} at size {best['vec_size']:.0f} "
        f"({best['speedup']:.2f}x vs scalar)",
        ha="right",
        va="bottom",
        fontsize=8,
        color="dimgrey",
        style="italic",
    )

    fig.suptitle(
        "SIMD vs Scalar:  fill-subtract-clamp  on  double  vectors",
        fontsize=14,
        y=0.97,
        color="dimgrey",
    )

    # --- Save ---
    out_dir = Path("./figures")
    out_dir.mkdir(exist_ok=True)
    fig.savefig(out_dir / "simd_bench.pdf", dpi=150, bbox_inches="tight")
    fig.savefig(out_dir / "simd_bench.png", dpi=150, bbox_inches="tight")
    plt.close()
    print("Saved to ./figures/simd_bench.{pdf,png}")


if __name__ == "__main__":
    main()
