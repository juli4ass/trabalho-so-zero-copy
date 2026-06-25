# 📖 GUIA PARA RODAR O PROJETO
**Driver Linux LKM — Zero-Copy I/O e DMA**

---

## 📋 OBJETIVO DESTE DOCUMENTO

Este guia foi escrito para que **qualquer pessoa** possa reproduzir o experimento desde a configuração do ambiente até a coleta dos resultados, em sua própria máquina, **sem precisar nos chamar**.

⏱️ **Tempo estimado de execução total:** 20 a 30 minutos.

---

## O QUE ESTE PROJETO FAZ

Implementa um **driver de kernel Linux (LKM)** que demonstra empiricamente as técnicas de:

- **Zero-Copy I/O** via `mmap()`
- **Direct Memory Access (DMA)** via `dma_alloc_coherent` + `dma_mmap_coherent`

Compara o desempenho do:

- `read()` tradicional (com cópia)
- `mmap()` (sem cópia)

Em ambiente isolado de tempo real (**SCHED_FIFO + mlockall + CPU pinning**).

---

# 1. REQUISITOS MÍNIMOS DO AMBIENTE

### ✅ Sistema operacional suportado

- Linux com kernel 5.x ou 6.x (recomendado: 6.8.0-azure ou superior)
- Ubuntu 22.04 LTS ou superior (recomendado), Debian, Fedora, ou similar
- Arquitetura x86_64 (64 bits)
- Acesso de administrador (sudo)

### ✅ Hardware mínimo

- 2 vCPUs ou mais
- 4 GB de RAM (recomendado: 8 GB)
- 10 GB de espaço em disco

### ✅ Pacotes que serão instalados

- build-essential (compilador GCC)
- linux-headers (cabeçalhos do kernel)
- linux-modules-extra (pacote estendido de módulos)
- Python 3.10+ com matplotlib, pandas e numpy
- Git

> ⚠️ **ATENÇÃO IMPORTANTE:** Se for usar uma VM Azure, configure-a como **Standard Security Type** (NÃO use Trusted Launch). VMs com Trusted Launch têm Secure Boot ativo, que bloqueia o carregamento de módulos não assinados. Esta limitação está documentada no item 6.2 do README.md.

---

# ⚙️ 2. CONFIGURAÇÃO DO AMBIENTE

## 🔹 Opção A — Ubuntu nativo ou WSL2

Se você já tem Linux instalado, pule direto para a **Seção 3**.

---

## 🔹 Opção B — VM no Azure (igual à usada no projeto)

1. Acesse https://portal.azure.com e faça login.
2. Clique em **Criar recurso → Máquina Virtual**.
3. Preencha:
   - **Nome:** trabalho-so-teste
   - **Região:** qualquer uma
   - **Security type:** Standard (IMPORTANTE!)
   - **Imagem:** Ubuntu Server 22.04 LTS - x64 Gen2
   - **Tamanho:** Standard_D2s_v3 (ou similar com 2 vCPU)
   - **Tipo de autenticação:** Senha
   - **Nome de usuário:** julia (ou outro de sua preferência)
   - **Senha:** defina uma forte
   - **Portas de entrada:** SSH (22) liberada
4. Clique em **Revisar + criar** e depois **Criar**.
5. Aguarde a criação (~3 minutos).
6. Anote o **IP público** da VM.
7. Conecte via SSH:

```bash
ssh julia@<IP-DA-VM>
# Substitua <IP-DA-VM> pelo IP público anotado
```

---

## 🔹 Opção C — VirtualBox/VMware (local)

1. Baixe a ISO do Ubuntu 22.04: https://ubuntu.com/download/server
2. Crie uma VM com 2 vCPU, 4 GB RAM, 20 GB disco.
3. Instale o Ubuntu seguindo o assistente.
4. Após instalado, faça login no terminal.

---

# 3. INSTALAÇÃO DAS DEPENDÊNCIAS

Execute os comandos abaixo **NA ORDEM**, dentro do terminal da máquina Linux.

## 3.1 — Atualizar o sistema

```bash
sudo apt-get update
```

```bash
sudo apt-get upgrade -y
```

> **Note:** O upgrade pode demorar alguns minutos. Se aparecer alguma tela azul perguntando sobre reiniciar serviços, selecione **Sim** (Tab → Enter).

## 3.2 — Instalar ferramentas de compilação e cabeçalhos do kernel

```bash
sudo apt-get install build-essential -y
```

```bash
sudo apt-get install linux-headers-$(uname -r) -y
```

```bash
sudo apt-get install linux-modules-extra-$(uname -r) -y
```

## 3.3 — Instalar Python e dependências para os gráficos

```bash
sudo apt-get install python3-pip -y
```

```bash
pip3 install matplotlib pandas numpy
```

> **Atenção:** Em distribuições mais novas (Ubuntu 24.04+), o `pip3` pode reclamar de `externally-managed-environment`. Nesse caso, use:
>
> ```bash
> pip3 install matplotlib pandas numpy --break-system-packages
> ```

## 3.4 — Instalar Git (se ainda não tiver)

```bash
sudo apt-get install git -y
```

## 3.5 — Verificar instalação

```bash
gcc --version       # Deve mostrar GCC 11+ ou compatível
make --version      # Deve mostrar GNU Make 4+
python3 --version   # Deve mostrar Python 3.10+
```

---

# 📂 4. BAIXAR O PROJETO

Use o Git para clonar (baixar) o repositório:

```bash
cd ~
```

```bash
git clone https://github.com/juli4ass/trabalho-so-zero-copy.git
```

```bash
cd trabalho-so-zero-copy
```

## 4.1 — Verificar arquivos baixados

```bash
ls -la
```

Você deverá ver:

| Arquivo | Descrição |
|---------|-----------|
| `ezdma_fake.c` | Código-fonte do driver de kernel (C) |
| `Makefile` | Script de compilação |
| `benchmark.c` | Benchmark simples (compara read vs mmap) |
| `benchmark_rt.c` | Benchmark de tempo real (com SCHED_FIFO) |
| `plot_results.py` | Gera gráficos dos resultados |
| `README.md` | Documentação principal |
| `CHANGELOG.md` | Histórico de versões |
| `LICENSE` | Licença GPL-2.0 |
| `DIAGNOSTICO_DMA_ENGINE.md` | Diagnóstico técnico |

---

# 5. COMPILAR O DRIVER E OS BENCHMARKS

Estando dentro da pasta `trabalho-so-zero-copy`, execute:

```bash
make
```

### Saída esperada (sucesso)

```
make -C /lib/modules/6.8.0-XX-generic/build M=/home/julia/trabalho-so-zero-copy modules
  CC [M]    ezdma_fake.o
  MODPOST   Module.symvers
  CC [M]    ezdma_fake.mod.o
  LD [M]    ezdma_fake.ko
gcc -O2 -Wall -o benchmark benchmark.c
gcc -O2 -Wall -o benchmark_rt benchmark_rt.c -lm
```

> **Warnings esperados (podem ser ignorados):** Em alguns kernels, podem aparecer avisos sobre `compiler differs` ou `BTF generation`. São avisos cosméticos. O importante é que o arquivo `ezdma_fake.ko` seja gerado.

### Verificar arquivos gerados

```bash
ls *.ko benchmark benchmark_rt
```

Deve mostrar:

```
benchmark  benchmark_rt  ezdma_fake.ko
```

---

### Possível erro: `No such file or directory: /lib/modules/...`

Significa que os cabeçalhos do kernel não estão instalados. Execute:

```bash
sudo apt-get install linux-headers-$(uname -r) -y
```

```bash
make clean && make
```

---

# 6. CARREGAR O DRIVER NO KERNEL

## 6.1 — Verificar que NÃO está carregado

```bash
lsmod | grep ezdma
```

> 📝 **Se aparecer algo:** Significa que o driver já está carregado. Descarregue primeiro: `sudo rmmod ezdma_fake`

## 6.2 — Carregar o módulo

```bash
sudo insmod ezdma_fake.ko
```

> **Sucesso:** Se voltar ao prompt **SEM mensagem de erro**, o driver foi carregado com sucesso.

---

###  Erro `Key was rejected by service`

Significa que o **Secure Boot** está ativo. Você tem 2 opções:

- **Opção 1:** Desativar o Secure Boot na BIOS/UEFI (varia por fabricante)
- **Opção 2:** Recriar a VM com **Security Type = Standard** (se for Azure)

> **Documentação completa:** Este problema está documentado no item 6 do README.md e no arquivo `DIAGNOSTICO_DMA_ENGINE.md`.

---

## 6.3 — Verificar logs do kernel (PROVA DO DMA REAL!)

```bash
sudo dmesg | tail -5
```

### Saída esperada

```
[XXXX.XX] ezdma_fake: loading out-of-tree module taints kernel
[XXXX.XX] ezdma_fake: module verification failed: signature missing (esperado)
[XXXX.XX] ezdma: inicializando v3.0 (DMA Real)
[XXXX.XX] ezdma: DMA buffer alocado | virt=0x... bus=0x000000XXXXXXXXXX size=4096
[XXXX.XX] ezdma: /dev/ezdma pronto (read + mmap, DMA-coherent real)
```

>  **PONTO-CHAVE DO TRABALHO:** A linha `bus=0x...` contém um **endereço de bus de 64 bits** emitido pelo kernel. Este é o endereço que um controlador DMA físico (NVMe, GPU, placa de rede) usaria para acessar a memória diretamente. **É a evidência empírica do DMA real implementado.**

---

#  7. VALIDAÇÃO FUNCIONAL

## 7.1 — Confirmar que `/dev/ezdma` foi criado

```bash
ls -l /dev/ezdma
```

Saída esperada:

```
crw-rw-rw- 1 root root 234, 0 [data] /dev/ezdma
```

>  **Sobre o major number 234:** Pode ser diferente em sua máquina (alocação dinâmica). O importante é o **c inicial** (character device) e a **permissão 0666** (qualquer usuário pode acessar).

## 7.2 — Leitura via `read()` tradicional

```bash
cat /dev/ezdma
```

Saída esperada:

```
DADO EM MEMORIA DMA-COHERENT - FURB Zero-Copy v3.0
```

---

# 8. EXECUÇÃO DOS BENCHMARKS

## 8.1 — Benchmark simples (read vs mmap)

```bash
./benchmark
```

**Tempo de execução:** ~5 segundos

### Saída esperada

```
=== Benchmark Zero-Copy vs Copia Tradicional ===
Iteracoes: 100000 | Buffer: 4096 bytes

[read()]  total: ~125.000.000 ns | media: ~1255 ns/op
[mmap()]  total:   ~9.000.000 ns | media:   ~90 ns/op

=== Resultado ===
mmap() foi ~92% mais rapido | Speedup: ~13.85x
```

> **Interpretação:** Speedup acima de 10x demonstra empiricamente a eliminação da cópia entre kernel e user space. Se houvesse cópia, os tempos seriam similares.

---

## 8.2 — Benchmark de tempo real

```bash
sudo ./benchmark_rt
```

**Tempo de execução:** ~10 segundos

### Saída esperada

```
=== Benchmark Real-Time: read() vs mmap() ===
Amostras: 100000 (warmup: 1000) | CPU pin: 1 | Prio FIFO: 80
Modo tempo real ATIVO (mlockall + SCHED_FIFO + CPU pin)

=== Resultados [read() tradicional] ===
  Min        : ~600 ns
  Media      : ~660 ns
  Mediana P50: ~700 ns
  P95        : ~700 ns
  P99        : ~700 ns
  P99.9      : ~2400 ns
  Max (WCET) : ~26000 ns
  Jitter (sd): ~245 ns

=== Resultados [mmap() Zero-Copy] ===
  Min        : ~0 ns (limite do clock_gettime)
  Media      : ~80 ns
  Mediana P50: ~100 ns
  P95        : ~100 ns
  P99        : ~100 ns
  P99.9      : ~200 ns
  Max (WCET) : ~16000 ns
  Jitter (sd): ~100 ns

Speedup medio    : ~8.5x
Reducao do WCET  : ~38%
```

> 📊 **Métricas-chave para tempo real:**
> - **WCET** (Worst Case Execution Time) — pior caso de execução
> - **Jitter** (desvio padrão) — variabilidade temporal
> - **P99.9** — percentil 99,9% (latência abaixo da qual 99,9% das amostras estão)

---

## 8.3 — Geração dos gráficos (opcional)

```bash
python3 plot_results.py
```

### Arquivos gerados

| Arquivo | Descrição |
|---------|-----------|
| `graph_histogram.png` | Histograma de latência |
| `graph_cdf.png` | CDF (Cumulative Distribution Function) |
| `graph_comparison.png` | Comparativo de métricas (escala log) |

---

# 🧹 9. CLEANUP — DESCARREGAR O DRIVER

Após os testes, descarregue o driver para verificar o ciclo de vida completo:

```bash
sudo rmmod ezdma_fake
```

## 9.1 — Verificar logs de descarregamento

```bash
sudo dmesg | tail -3
```

Saída esperada:

```
[XXXX.XX] ezdma [MMAP]: pagina DMA-coherent mapeada (uso anterior)
[XXXX.XX] ezdma: descarregado (v3.0 DMA Real)
```

## 9.2 — Confirmar que `/dev/ezdma` foi removido

```bash
ls /dev/ezdma 2>&1
```

Saída esperada:

```
ls: cannot access '/dev/ezdma': No such file or directory
```

> ✅ **Ciclo de vida completo:** Driver foi carregado → alocou memória DMA → criou device → recebeu chamadas → liberou memória → destruiu device → descarregou. **Sem leaks ou panics.**

---

# ⚡ 10. SCRIPT TUDO-EM-UM (PARA TESTE RÁPIDO)

Para executar **TODOS os passos de uma vez** (ideal para validação rápida):

```bash
sudo apt-get update && \
sudo apt-get install -y build-essential linux-headers-$(uname -r) \
                       linux-modules-extra-$(uname -r) python3-pip git && \
pip3 install matplotlib pandas numpy && \
cd ~ && \
git clone https://github.com/juli4ass/trabalho-so-zero-copy.git && \
cd trabalho-so-zero-copy && \
make && \
sudo insmod ezdma_fake.ko && \
sudo dmesg | tail -5 && \
./benchmark && \
sudo ./benchmark_rt && \
sudo rmmod ezdma_fake && \
sudo dmesg | tail -3
```

> ⏱️ **Tempo total estimado:** Em uma VM Azure Standard_D2s_v3: aproximadamente **5 a 8 minutos** do zero ao primeiro benchmark.

---

# 🔧 11. SOLUÇÃO DE PROBLEMAS COMUNS

## Problema 1: `make` falha com `No rule to make target`

**Causa:** cabeçalhos do kernel ausentes.

**Solução:**

```bash
sudo apt-get install linux-headers-$(uname -r) -y
make clean && make
```

---

## Problema 2: `insmod` falha com `Key was rejected by service`

**Causa:** Secure Boot ativo, exige módulos assinados.

**Soluções:**

a. Desativar Secure Boot na BIOS/UEFI

b. Em Azure: recriar VM com **Security Type = Standard**

c. Em outras VMs: assinar o módulo com **MOK (Machine Owner Key)** — ver documentação Linux

---

## Problema 3: `insmod` falha com `File exists`

**Causa:** o módulo já está carregado.

**Solução:**

```bash
sudo rmmod ezdma_fake
sudo insmod ezdma_fake.ko
```

---

## Problema 4: `benchmark_rt` falha com `Operation not permitted`

**Causa:** SCHED_FIFO requer privilégios. Esqueceu o `sudo`.

**Solução:** usar sempre `sudo ./benchmark_rt`

---

## Problema 5: `pip3` reclama de `externally-managed-environment`

**Causa:** distribuições novas protegem o Python do sistema.

**Solução:**

```bash
pip3 install matplotlib pandas numpy --break-system-packages
```

Ou crie um ambiente virtual:

```bash
python3 -m venv venv && source venv/bin/activate
```

---

## Problema 6: WCET ou jitter muito altos

**Causa:** máquina com carga alta de outros processos, ou hipervisor com interferência.

**Solução:** rodar com menos processos abertos, em VM dedicada, ou bare-metal. Os números absolutos podem variar, mas as **tendências** (mmap mais rápido e previsível) se mantêm.

---

## Problema 7: Os números são diferentes dos do README

**Causa:** variações naturais entre execuções (cache, hypervisor, hardware).

**Solução:** o que importa são as **TENDÊNCIAS**, não valores absolutos. Os CSVs brutos (`results_*.csv`) e o arquivo `resultado_rt.txt` mostram os resultados originais coletados no ambiente do trabalho.

---

# ✅ 12. CHECKLIST DE VALIDAÇÃO

Marque cada item conforme for executando:

- [ ] 1. Ambiente Linux preparado (Ubuntu 22.04 ou similar)
- [ ] 2. `build-essential` instalado (`gcc --version` funciona)
- [ ] 3. `linux-headers` instalado para o kernel atual
- [ ] 4. Python 3.10+ com matplotlib/pandas/numpy
- [ ] 5. Repositório clonado com sucesso (`git clone`)
- [ ] 6. `make` compilou sem erros (`ezdma_fake.ko` criado)
- [ ] 7. `sudo insmod ezdma_fake.ko` carregou sem erro
- [ ] 8. `dmesg` mostra `inicializando v3.0 (DMA Real)`
- [ ] 9. `dmesg` mostra `bus=0x...` (bus address de 64 bits)
- [ ] 10. `/dev/ezdma` criado com permissão 0666
- [ ] 11. `cat /dev/ezdma` retorna a string esperada
- [ ] 12. `./benchmark` executa e mostra speedup > 5x
- [ ] 13. `sudo ./benchmark_rt` executa e mostra WCET < 50µs no mmap
- [ ] 14. `python3 plot_results.py` gera 3 PNGs
- [ ] 15. `sudo rmmod ezdma_fake` remove o driver limpo
- [ ] 16. `dmesg` mostra `descarregado (v3.0 DMA Real)`

> **Se TODOS os 16 itens deram certo:** PARABÉNS! O experimento foi reproduzido com sucesso. O trabalho funciona em seu ambiente exatamente como descrito na documentação. Os resultados podem variar em valores absolutos devido a diferenças de hardware/hipervisor, mas as **tendências** são as mesmas.

---

## Obrigada por testar nosso trabalho!

