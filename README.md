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
