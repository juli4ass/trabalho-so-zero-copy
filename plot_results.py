#!/usr/bin/env python3
"""
plot_results.py - Gera 3 graficos a partir dos CSVs do benchmark_rt

Saidas:
  - graph_histogram.png  : distribuicao de latencia (read vs mmap)
  - graph_cdf.png        : CDF (cumulative distribution function)
  - graph_comparison.png : barras com metricas-chave (escala log)

Uso: python3 plot_results.py
"""
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# === Carrega os CSVs ===
read_data = pd.read_csv("results_read.csv")["sample_ns"].values
mmap_data = pd.read_csv("results_mmap.csv")["sample_ns"].values

def stats(data):
    return {
        "min":    int(np.min(data)),
        "mean":   float(np.mean(data)),
        "p50":    float(np.percentile(data, 50)),
        "p95":    float(np.percentile(data, 95)),
        "p99":    float(np.percentile(data, 99)),
        "p999":   float(np.percentile(data, 99.9)),
        "max":    int(np.max(data)),
        "stddev": float(np.std(data)),
    }

s_read = stats(read_data)
s_mmap = stats(mmap_data)

# === 1) Histograma (limitado ao P99 para escala legivel) ===
fig, ax = plt.subplots(figsize=(10, 5))
upper = max(s_read["p99"], s_mmap["p99"])
ax.hist(read_data[read_data <= upper], bins=80, alpha=0.6,
        label=f'read()  (media={s_read["mean"]:.0f} ns)', color='tab:red')
ax.hist(mmap_data[mmap_data <= upper], bins=80, alpha=0.6,
        label=f'mmap()  (media={s_mmap["mean"]:.0f} ns)', color='tab:blue')
ax.set_xlabel("Latencia (ns)")
ax.set_ylabel("Frequencia (amostras)")
ax.set_title("Distribuicao de Latencia: read() vs mmap() Zero-Copy\n(limitado ao P99 para escala)")
ax.legend()
ax.grid(alpha=0.3)
plt.tight_layout()
plt.savefig("graph_histogram.png", dpi=120)
plt.close()
print("[OK] graph_histogram.png gerado")

# === 2) CDF (criterio classico de analise de tempo real) ===
fig, ax = plt.subplots(figsize=(10, 5))
for data, label, color in [(read_data, "read() tradicional", "tab:red"),
                            (mmap_data, "mmap() Zero-Copy",   "tab:blue")]:
    sorted_d = np.sort(data)
    cdf = np.arange(1, len(sorted_d) + 1) / len(sorted_d)
    ax.plot(sorted_d, cdf, label=label, color=color, linewidth=2)
ax.set_xscale("log")
ax.set_xlabel("Latencia (ns) - escala log")
ax.set_ylabel("CDF (probabilidade acumulada)")
ax.set_title("CDF de Latencia: criterio chave para Sistemas de Tempo Real")
ax.axhline(0.99,  color='gray', linestyle='--', alpha=0.5, label="P99")
ax.axhline(0.999, color='gray', linestyle=':',  alpha=0.5, label="P99.9")
ax.legend()
ax.grid(alpha=0.3, which="both")
plt.tight_layout()
plt.savefig("graph_cdf.png", dpi=120)
plt.close()
print("[OK] graph_cdf.png gerado")

# === 3) Barras comparativas com metricas-chave (escala log) ===
fig, ax = plt.subplots(figsize=(10, 5))
metrics = ["mean", "p50", "p95", "p99", "p999", "max", "stddev"]
labels  = ["Media", "P50", "P95", "P99", "P99.9", "Max (WCET)", "Jitter (sd)"]
x = np.arange(len(metrics))
w = 0.35
r_vals = [s_read[m] for m in metrics]
m_vals = [s_mmap[m] for m in metrics]
ax.bar(x - w/2, r_vals, w, label="read()", color="tab:red")
ax.bar(x + w/2, m_vals, w, label="mmap()", color="tab:blue")
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=15)
ax.set_ylabel("Latencia (ns) - escala log")
ax.set_yscale("log")
ax.set_title("Metricas de Latencia: read() vs mmap() Zero-Copy")
ax.legend()
ax.grid(alpha=0.3, axis="y", which="both")
plt.tight_layout()
plt.savefig("graph_comparison.png", dpi=120)
plt.close()
print("[OK] graph_comparison.png gerado")

# === Resumo no terminal ===
print("\n=== Resumo gerado pelos graficos ===")
print(f"Speedup medio    : {s_read['mean']/s_mmap['mean']:.2f}x")
print(f"Reducao do WCET  : {(1 - s_mmap['max']/s_read['max'])*100:.1f}%  "
      f"({s_read['max']} -> {s_mmap['max']} ns)")
print(f"Reducao do jitter: {(1 - s_mmap['stddev']/s_read['stddev'])*100:.1f}%  "
      f"({s_read['stddev']:.0f} -> {s_mmap['stddev']:.0f} ns)")
print(f"P99 read         : {s_read['p99']:.0f} ns")
print(f"P99 mmap         : {s_mmap['p99']:.0f} ns")
print(f"P99.9 read       : {s_read['p999']:.0f} ns")
print(f"P99.9 mmap       : {s_mmap['p999']:.0f} ns")
print("\nGraficos salvos:")
print("  - graph_histogram.png  (distribuicao de latencia)")
print("  - graph_cdf.png        (CDF - tempo real)")
print("  - graph_comparison.png (barras comparativas)")
