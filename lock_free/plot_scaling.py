# /// script
# requires-python = ">=3.12"
# dependencies = ["matplotlib", "seaborn", "numpy", "pandas"]
# ///
"""
Line chart: throughput scaling with thread count for each type size and workload.

Rows: type sizes (int, Type64, Type256)
Columns: workloads (push, mixed, pop)
Each panel: thread count on x, throughput (M ops/s) on y, two lines (LockFree, Mutex)
"""

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from matplotlib import gridspec


def main():
    sns.set_theme(font_scale=1.0, style="whitegrid", font="DejaVu Sans")

    data_path = Path(__file__).resolve().parent.parent / "build" / "bench_results.csv"
    df = pd.read_csv(data_path).dropna()

    lf = df[["type", "threads", "workload", "lockfree_ops"]].copy()
    lf.columns = ["type", "threads", "workload", "ops"]
    lf["impl"] = "LockFree"

    mx = df[["type", "threads", "workload", "mutex_ops"]].copy()
    mx.columns = ["type", "threads", "workload", "ops"]
    mx["impl"] = "Mutex"

    plot_df = pd.concat([lf, mx], ignore_index=True)
    plot_df["ops_m"] = plot_df["ops"] / 1e6

    type_sizes = ["int", "Type64", "Type256"]
    type_labels = {"int": "int (4 B)", "Type64": "Type64 (64 B)", "Type256": "Type256 (256 B)"}
    workloads = ["push", "mixed", "pop"]
    workload_labels = {"push": "Push-only", "mixed": "Push+Pop (balanced)", "pop": "Pop-only"}

    n_rows, n_cols = 3, 3
    fig = plt.figure(figsize=(16, 12), dpi=150)
    gs = gridspec.GridSpec(n_rows, n_cols)
    gs.update(wspace=0.12, hspace=0.30, left=0.07, right=0.99, top=0.93, bottom=0.07)

    axes_list = [plt.subplot(gs[i, j]) for i in range(n_rows) for j in range(n_cols)]

    impl_color = {"LockFree": "#4575b4", "Mutex": "#d73027"}
    impl_style = {"LockFree": "-", "Mutex": "--"}
    impl_marker = {"LockFree": "o", "Mutex": "s"}
    workload_marker = {"push": "o", "mixed": "^", "pop": "D"}

    for idx, ax in enumerate(axes_list):
        row, col = divmod(idx, n_cols)
        tsize = type_sizes[row]
        workload = workloads[col]

        subset = plot_df[(plot_df["type"] == tsize) & (plot_df["workload"] == workload)]

        for impl in ["LockFree", "Mutex"]:
            imp_df = subset[subset["impl"] == impl].sort_values("threads")
            ax.plot(
                imp_df["threads"],
                imp_df["ops_m"],
                color=impl_color[impl],
                linestyle=impl_style[impl],
                marker=impl_marker[impl],
                markerfacecolor=impl_color[impl],
                markeredgecolor="white",
                markeredgewidth=0.6,
                linewidth=2,
                markersize=8,
                label=impl,
                zorder=4,
            )

            for _, row_data in imp_df.iterrows():
                ax.text(
                    row_data["threads"],
                    row_data["ops_m"],
                    f'{row_data["ops_m"]:.1f}',
                    ha="center",
                    va="bottom" if impl == "LockFree" else "top",
                    fontsize=6,
                    color="dimgrey",
                    weight="semibold",
                )

        label_chr = chr(ord("a") + idx)
        ax.set_title(
            rf"$\bf{{({label_chr})}}$  {type_labels[tsize]} — {workload_labels[workload]}",
            loc="left",
            fontsize=11,
            pad=6,
        )

        if row == n_rows - 1:
            ax.set_xlabel("Thread count", fontsize=10, labelpad=5, color="dimgrey")
        else:
            ax.set_xlabel("")

        if col == 0:
            ax.set_ylabel("Throughput (M ops/s)", fontsize=10, labelpad=6, color="dimgrey")
        else:
            ax.set_ylabel("")
            ax.tick_params(labelleft=False)

        ax.set_xticks([1, 2, 4, 8])
        ax.set_xticklabels(["1", "2", "4", "8"])

        y_min = subset["ops_m"].min()
        y_max = subset["ops_m"].max()
        y_pad = (y_max - y_min) * 0.12 if y_max > y_min else y_max * 0.2
        ax.set_ylim(max(0, y_min - y_pad), y_max + y_pad)

        ax.grid(axis="y", alpha=0.5, linewidth=0.7)
        ax.tick_params(axis="both", which="both", length=0, labelcolor="dimgrey")
        ax.patch.set_edgecolor("lightgrey")
        ax.patch.set_linewidth(0.8)

        if idx == 0:
            ax.legend(
                frameon=True,
                facecolor="white",
                framealpha=0.85,
                edgecolor="lightgrey",
                labelcolor="dimgrey",
                loc="upper left",
                fontsize=9,
            )
        else:
            leg = ax.get_legend()
            if leg:
                leg.remove()

    sns.despine(left=True, bottom=True)

    fig.suptitle(
        "Throughput Scaling with Thread Count — LockFreeStack vs MutexStack",
        fontsize=14,
        y=0.98,
        color="dimgrey",
    )

    out_dir = Path("./figures")
    out_dir.mkdir(exist_ok=True)
    fig.savefig(out_dir / "scaling_overview.pdf", dpi=150, bbox_inches="tight")
    fig.savefig(out_dir / "scaling_overview.png", dpi=150, bbox_inches="tight")
    plt.close()
    print("Saved to ./figures/scaling_overview.{pdf,png}")


if __name__ == "__main__":
    main()
