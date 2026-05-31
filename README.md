# Driver Zero-Copy I/O - ezdma

Trabalho da disciplina **Sistemas Operacionais** - FURB / Sistemas de Informacao
Autora: **Julia da Assuncao Silva** e grupo

## Tema
Otimizacao de I/O em Sistemas de Tempo Real:
Implementacao de Zero-Copy e DMA.

## Conteudo
- `ezdma_fake.c` - Driver de Kernel Linux (LKM) com `read()` + `mmap()`
- `Makefile` - Compilacao out-of-tree
- `benchmark.c` - Comparativo simples `read()` vs `mmap()`
- `benchmark_rt.c` - Benchmark **Real-Time** (SCHED_FIFO + mlockall + CPU pin)
- `plot_results.py` - Gera 3 graficos a partir dos CSVs
- `results_*.csv` - 100.000 amostras de cada caminho (read/mmap)
- `graph_*.png` - Histograma, CDF e barras comparativas

## Como rodar

```bash
sudo apt-get install build-essential linux-headers-$(uname -r) -y
make
sudo insmod ezdma_fake.ko
sudo ./benchmark_rt
python3 plot_results.py
sudo rmmod ezdma_fake
```

## Tecnicas de Tempo Real aplicadas (Semana 2)

| Tecnica | Funcao |
|---|---|
| `mlockall(MCL_CURRENT \| MCL_FUTURE)` | Trava paginas em RAM (sem page faults) |
| `SCHED_FIFO` prio 80 | Escalonamento de tempo real |
| `sched_setaffinity(CPU 1)` | Isola thread em uma unica CPU |
| Warm-up (1000 iter) | Aquece cache antes da medicao |
| 100.000 amostras | Base estatistica robusta |

## Resultados (Ubuntu 22.04 / Azure Standard_D2s_v3)

### Benchmark Real-Time (Semana 2)

| Metrica | `read()` | `mmap()` Zero-Copy | Ganho |
|---|---:|---:|---:|
| Min | 600 ns | 100 ns | 6x |
| Media | 1.009,81 ns | 139,53 ns | 7,24x |
| Mediana (P50) | 1.100 ns | 100 ns | 11x |
| P95 | 1.200 ns | 200 ns | 6x |
| P99 | 1.201 ns | 200 ns | 6x |
| P99.9 | 9.101 ns | 300 ns | **30x** |
| **Max (WCET)** | **1.084.263 ns** | **32.501 ns** | **97% menor** |
| **Jitter (stddev)** | **3.978,53 ns** | **210,74 ns** | **94,7% menor** |

> **Insight chave para Sistemas de Tempo Real:** o WCET (Worst Case Execution Time) caiu de ~1 ms para ~32 us — uma reducao de **97%**. Combinado com a reducao de **94,7%** no jitter, isso significa comportamento drasticamente mais previsivel, requisito fundamental em aplicacoes hard real-time.

### Graficos

#### Histograma de latencia
![Histograma](graph_histogram.png)

#### CDF (criterio classico de tempo real)
![CDF](graph_cdf.png)

#### Comparativo de metricas (escala log)
![Comparativo](graph_comparison.png)

## Ambiente
- Ubuntu 22.04 LTS
- Kernel: 6.8.0-1052-azure
- VM: Azure Standard_D2s_v3 (Standard Security)
- Compilador: gcc 11.4.0
- Python: 3.10 + matplotlib + pandas + numpy

## Estrutura do projeto

```
trabalho-so-zero-copy/
├── ezdma_fake.c         # Driver LKM
├── Makefile             # Build out-of-tree
├── benchmark.c          # Bench simples
├── benchmark_rt.c       # Bench Real-Time
├── plot_results.py      # Gera graficos
├── results_*.csv        # Amostras brutas (100k cada)
├── graph_*.png          # Graficos gerados
├── resultado_*.txt      # Saidas textuais
└── README.md            # Este arquivo
```
