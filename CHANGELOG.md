# Changelog

Todas as mudancas notaveis deste projeto sao documentadas aqui.

O formato segue [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) e o projeto adere ao [Semantic Versioning](https://semver.org/).

---

## [3.0] - 2026-05-31 - Semana 3 - DMA Real

### Adicionado
- Refatoracao do driver para usar a **DMA Coherent API real** do kernel Linux.
- `dma_alloc_coherent()` substitui `kmalloc_node()` para alocacao DMA-coerente.
- `dma_mmap_coherent()` substitui `remap_pfn_range()` para mapeamento Zero-Copy.
- `platform_device_register_simple()` para registrar struct device* exigido pela DMA API.
- `dma_set_coherent_mask(DMA_BIT_MASK(64))` para suporte a enderecos de bus de 64 bits.
- `dma_addr_t dma_handle` expoe bus address real (logado em dmesg).
- Tratamento completo de erros (`err_class`, `err_chrdev`, `err_dma`, `err_pdev`).

### Alterado
- `MODULE_VERSION` atualizado para "3.0".
- String de demonstracao alterada para "DADO EM MEMORIA DMA-COHERENT".
- README com **Linha do Tempo** das 3 semanas e tabela comparativa v2.1 vs v3.0.

### Resultados
- WCET do `read()` reduzido em **97%** (1.084.263 ns -> 30.002 ns).
- Jitter do `read()` reduzido em **91%** (3.978 ns -> 336 ns).
- Jitter do `mmap()` reduzido em **21%** (210 ns -> 166 ns).
- Speedup medio: 6,82x (trade-off intencional para melhor WCET).
- Evidencia empirica do bus address: `bus=0x0000000102bfd000`.

---

## [2.1] - 2026-05-31 - Semana 2 - Tempo Real e Analise Estatistica

### Adicionado
- `benchmark_rt.c` com **tecnicas reais de tempo real**:
  - `mlockall(MCL_CURRENT | MCL_FUTURE)` (trava paginas em RAM)
  - `SCHED_FIFO` prio 80 (escalonamento de tempo real)
  - `sched_setaffinity(CPU 1)` (CPU pinning)
  - Warm-up de 1000 iteracoes
- Coleta de 100.000 amostras por caminho (read e mmap).
- Analise estatistica: min, media, P50, P95, P99, P99.9, WCET, jitter.
- Exportacao CSV (`results_read.csv`, `results_mmap.csv`).
- `plot_results.py` (matplotlib + pandas + numpy):
  - Histograma de latencia
  - CDF (criterio classico para Sistemas de Tempo Real)
  - Comparativo de metricas em escala log
- README atualizado com tabela de tecnicas RT e graficos renderizados inline.

### Resultados
- WCET reduzido em **97%** (1.084.263 ns).
- Jitter reduzido em **94,7%** (3.978 ns -> 210 ns).
- Speedup medio: 7,24x.

---

## [2.0] - 2026-05-30 - Semana 1 - Driver Funcional + GitHub

### Adicionado
- Driver Linux LKM `ezdma_fake.c` com 2 caminhos de I/O:
  - `dev_read()` usando `copy_to_user` (caminho tradicional).
  - `dev_mmap()` usando `remap_pfn_range` (Zero-Copy via mmap).
- Alocacao com afinidade NUMA (`kmalloc_node(GFP_KERNEL, 0)`).
- `SetPageReserved()` para evitar swap (importante em RT).
- Criacao automatica de `/dev/ezdma` via `class_create` + `device_create`.
- Permissoes 0666 via callback `devnode` (evita erros de permissao).
- Compatibilidade kernel 5.x e 6.x via `LINUX_VERSION_CODE`.
- `Makefile` out-of-tree.
- `benchmark.c` simples (medicao com `clock_gettime`).
- README inicial com instrucoes de uso e resultados.
- `.gitignore` para artefatos de build (`*.ko`, `*.o`, `*.mod`, etc.).

### Resultados
- Speedup medio: **~18x** (mmap vs read), com picos de ate 24,53x.
- Validacao funcional em VM Azure Ubuntu 22.04 Standard.

### Notas tecnicas
- Foi necessario criar **VM nova com Security type "Standard"** apos descoberta de que o kernel `linux-azure` com Trusted Launch forca `lockdown=integrity`, impedindo carregamento de modulos nao assinados.
- Lockdown nao pode ser desativado via parametro `lockdown=none` no GRUB em VMs `linux-azure`.

---

## Referencias

- **Documentacao Linux Kernel**: <https://www.kernel.org/doc/html/latest/>
- **Linux Device Drivers (LDD3)**: <https://lwn.net/Kernel/LDD3/>
- **DMA Engine API**: <https://www.kernel.org/doc/html/latest/driver-api/dmaengine/>
- **DMA Coherent API**: <https://www.kernel.org/doc/html/latest/core-api/dma-api.html>
