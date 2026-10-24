import json
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

RESULTS_DIR = Path(__file__).resolve().parent / "results"
with open(RESULTS_DIR / "benchmarks.json") as f:
    data = json.load(f)

circuits = [d["circuit"] for d in data]
zx_times = [d["zx_time_ms"] for d in data]
pyzx_times = [d["pyzx_time_ms"] for d in data]
qsyn_times = [d.get("qsyn_time_ms", 0.0) for d in data]

orig_t = [d["orig_t"] for d in data]
zx_t = [d["zx_t"] for d in data]
pyzx_t = [d["pyzx_t"] for d in data]
qsyn_t = [d.get("qsyn_t", 0) for d in data]

plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')

# 1. Runtime Comparison (Log Scale: 3-way)
fig, ax1 = plt.subplots(figsize=(12, 6), dpi=300)
x = np.arange(len(circuits))
width = 0.26

rects1 = ax1.bar(x - width, zx_times, width, label='zx-reduce (Our C++20)', color='#1f77b4', alpha=0.9)
rects2 = ax1.bar(x, qsyn_times, width, label='qsyn (NTU C++20)', color='#9467bd', alpha=0.9)
rects3 = ax1.bar(x + width, pyzx_times, width, label='PyZX (Python Baseline)', color='#ff7f0e', alpha=0.9)

ax1.set_ylabel('Execution Time (ms, log scale)', fontsize=12, fontweight='bold')
ax1.set_title('Quantum Circuit Optimization Runtime: zx-reduce vs qsyn vs PyZX', fontsize=14, fontweight='bold', pad=15)
ax1.set_xticks(x)
ax1.set_xticklabels(circuits, rotation=30, ha='right', fontsize=10)
ax1.set_yscale('log')
ax1.legend(fontsize=11)
ax1.grid(True, which="both", ls="--", alpha=0.5)

# Annotate speedup of zx-reduce vs PyZX and vs qsyn
for i, (z, q, p) in enumerate(zip(zx_times, qsyn_times, pyzx_times)):
    sp_p = p / z if z > 0 else 0
    sp_q = q / z if z > 0 else 0
    ax1.annotate(f"{sp_p:.0f}x py\n{sp_q:.0f}x qs",
                 xy=(i - width, z),
                 xytext=(0, 6),
                 textcoords="offset points",
                 ha='center', va='bottom',
                 fontsize=7.5, fontweight='bold', color='#1f77b4')

fig.tight_layout()
fig.savefig(RESULTS_DIR / "runtime_comparison.png")
plt.close(fig)
print("Saved runtime_comparison.png")

# 2. T-Count Reduction Comparison (4-way: Orig, zx-reduce, PyZX, qsyn)
fig, ax2 = plt.subplots(figsize=(12, 6), dpi=300)
w4 = 0.20
rects_orig = ax2.bar(x - 1.5*w4, orig_t, w4, label='Original T-count', color='#7f7f7f', alpha=0.8)
rects_zx = ax2.bar(x - 0.5*w4, zx_t, w4, label='zx-reduce T-count', color='#1f77b4', alpha=0.9)
rects_qsyn = ax2.bar(x + 0.5*w4, qsyn_t, w4, label='qsyn T-count', color='#9467bd', alpha=0.9)
rects_py = ax2.bar(x + 1.5*w4, pyzx_t, w4, label='PyZX T-count', color='#ff7f0e', alpha=0.9)

ax2.set_ylabel('T-Count', fontsize=12, fontweight='bold')
ax2.set_title('T-Count Reduction: Original vs zx-reduce vs qsyn vs PyZX', fontsize=14, fontweight='bold', pad=15)
ax2.set_xticks(x)
ax2.set_xticklabels(circuits, rotation=30, ha='right', fontsize=10)
ax2.legend(fontsize=11)
ax2.grid(True, ls="--", alpha=0.5)

fig.tight_layout()
fig.savefig(RESULTS_DIR / "t_count_reduction.png")
plt.close(fig)
print("Saved t_count_reduction.png")
