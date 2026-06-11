# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "matplotlib",
#     "seaborn",
#     "numpy",
#     "pandas",
# ]
# ///
"""
Plot LockFreeStack vs MutexStack benchmark results as a 3x3 multi-panel figure.

Rows: workloads (push, mixed, pop)
Columns: type sizes (int, Type64, Type256)
Each panel: grouped bars, x=threads, hue=implementation, y=throughput (M ops/s)
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
    data_path = Path(__file__).resolve().parent.parent / "build" / "bench_results.csv"
    df = pd.read_csv(data_path).dropna()
    df["impl"] = "LockFree"
    lf = df[["type", "threads", "workload", "lockfree_ops"]].copy()
    lf.columns = ["type", "threads", "workload", "ops"]
    lf["impl"] = "LockFree"

    mx = df[["type", "threads", "workload", "mutex_ops"]].copy()
    mx.columns = ["type", "threads", "workload", "ops"]
    mx["impl"] = "Mutex"

    plot_df = pd.concat([lf, mx], ignore_index=True)
    # Convert to Mops/s
    plot_df["ops_m"] = plot_df["ops"] / 1e6

    workloads = ["push", "mixed", "pop"]
    workload_labels = {"push": "Push-only", "mixed": "Push+Pop (balanced)", "pop": "Pop-only"}
    type_sizes = ["int", "Type64", "Type256"]
    type_labels = {"int": "int (4 B)", "Type64": "Type64 (64 B)", "Type256": "Type256 (256 B)"}

    # --- Layout: 3 rows (workloads) x 3 cols (type sizes) ---
    n_rows, n_cols = 3, 3
    fig = plt.figure(figsize=(16, 12), dpi=150)
    gs = gridspec.GridSpec(n_rows, n_cols)
    gs.update(wspace=0.10, hspace=0.30, left=0.07, right=0.99, top=0.93, bottom=0.07)

    axes_list = [plt.subplot(gs[i, j]) for i in range(n_rows) for j in range(n_cols)]

    pal = {"LockFree": "#4575b4", "Mutex": "#d73027"}
    hatch = {"LockFree": "", "Mutex": "//"}
    markers = {"LockFree": "o", "Mutex": "s"}

    all_ymax = plot_df.groupby("workload")["ops_m"].max()
    pd.options.mode.chained_assignment = None

    for idx, ax in enumerate(axes_list):
        row, col = divmod(idx, n_cols)
        workload = workloads[row]
        tsize = type_sizes[col]

        subset = plot_df[(plot_df["workload"] == workload) & (plot_df["type"] == tsize)]

        # --- Grouped bar chart ---
        sns.barplot(
            data=subset,
            x="threads",
            y="ops_m",
            hue="impl",
            palette=pal,
            ax=ax,
            edgecolor="white",
            linewidth=0.6,
            width=0.7,
        )

        # --- Value labels on bars ---
        for p in ax.patches:
            h = p.get_height()
            if h > 0:
                ax.text(
                    p.get_x() + p.get_width() / 2,
                    h + all_ymax[workload] * 0.015,
                    f"{h:.1f}",
                    ha="center",
                    va="bottom",
                    fontsize=7,
                    color="dimgrey",
                    weight="semibold",
                )

        # --- Overlay markers for the two implementations ---
        for impl, color in pal.items():
            imp_df = subset[subset["impl"] == impl]
            ax.scatter(
                x=imp_df["threads"].astype(str),
                y=imp_df["ops_m"],
                color=color,
                marker=markers[impl],
                s=50,
                zorder=5,
                edgecolors="white",
                linewidths=0.6,
            )

        # --- Subplot title ---
        label_chr = chr(ord("a") + idx)
        parts = [r"$\bf{(" + label_chr + r")}$" + f"  {type_labels[tsize]} — {workload_labels[workload]}"]
        ax.set_title(parts[0], loc="left", fontsize=11, pad=6)

        # --- Axis labels ---
        if row == n_rows - 1:
            ax.set_xlabel("Threads", fontsize=10, labelpad=5, color="dimgrey")
        else:
            ax.set_xlabel("")

        if col == 0:
            ax.set_ylabel("Throughput (M ops/s)", fontsize=10, labelpad=6, color="dimgrey")
        else:
            ax.set_ylabel("")
            ax.tick_params(labelleft=False)

        # --- Styling ---
        ax.grid(axis="y", alpha=0.5, linewidth=0.7)
        ax.tick_params(axis="both", which="both", length=0, labelcolor="dimgrey")
        ax.patch.set_edgecolor("lightgrey")
        ax.patch.set_linewidth(0.8)

        # Consistent y range
        y_top = all_ymax[workload] * 1.25
        ax.set_ylim(0, y_top)

        # Legend on first panel only
        if idx == 0:
            ax.legend(
                frameon=True,
                facecolor="white",
                framealpha=0.85,
                edgecolor="lightgrey",
                labelcolor="dimgrey",
                loc="upper right",
                fontsize=9,
                title="Implementation",
                title_fontsize=9,
            )
        else:
            ax.get_legend().remove() if ax.get_legend() else None

        # Horizontal line at y=0
        ax.axhline(y=0, color="lightgrey", linewidth=0.6, zorder=0)

    sns.despine(left=True, bottom=True)

    # --- Figure-level insight annotation (compute from data) ---
    # Biggest speedup across all configs
    df_speedup = df.copy()
    df_speedup["speedup"] = df_speedup["lockfree_ops"] / df_speedup["mutex_ops"]
    best = df_speedup.loc[df_speedup["speedup"].idxmax()]
    worst = df_speedup.loc[df_speedup["speedup"].idxmin()]

    fig.text(
        0.99,
        0.01,
        f"LockFree excels at push-heavy workloads (up to {best['speedup']:.0f}x with {best['type']}, "
        f"{best['threads']}t). "
        f"Worst case: {worst['workload']}/{worst['type']}/{worst['threads']}t "
        f"({worst['speedup']:.2f}x)",
        ha="right",
        va="bottom",
        fontsize=8,
        color="dimgrey",
        style="italic",
    )

    fig.suptitle(
        "LockFreeStack vs MutexStack — Throughput by Type Size, Workload, and Thread Count",
        fontsize=14,
        y=0.98,
        color="dimgrey",
    )

    # --- Save ---
    Path("./figures").mkdir(exist_ok=True)
    fig.savefig("./figures/bench_overview.pdf", dpi=150, bbox_inches="tight")
    fig.savefig("./figures/bench_overview.png", dpi=150, bbox_inches="tight")
    plt.close()
    print("Saved to ./figures/bench_overview.{pdf,png}")


if __name__ == "__main__":
    main()
