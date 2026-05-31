# Diagnostico: Tentativa de implementacao da camada de transferencia DMA assincrona

**Data:** 31/05/2026
**Ambiente:** Azure VM Standard_D2s_v3, Ubuntu 22.04 LTS, kernel 6.8.0-1052-azure
**Driver afetado:** ezdma_fake.c v3.0

---

## Contexto

O driver `ezdma_fake.c v3.0` implementa a **DMA Coherent API real** do kernel Linux (`dma_alloc_coherent`, `dma_mmap_coherent`, `platform_device`, `dma_set_coherent_mask`). A proxima evolucao natural seria adicionar a camada de **transferencia DMA assincrona** via DMA Engine API (`dma_request_channel` + `dmaengine_prep_dma_memcpy`), permitindo copia memory-to-memory sem intervencao da CPU.

Este documento registra a tentativa, o diagnostico realizado, e a conclusao tecnica sobre por que essa camada nao pode ser implementada no ambiente Azure utilizado.

---

## Tentativa realizada

### 1. Verificacao de canais DMA disponiveis (estado inicial)

```bash
ls /sys/class/dma/
```

**Resultado:** vazio (nenhum canal DMA exposto pelo kernel).

### 2. Carregamento manual de provedores DMA

```bash
sudo modprobe dmaengine
sudo modprobe ioat
sudo modprobe dmatest
```

**Resultado:** todos os comandos executaram silenciosamente (sem erro e sem sucesso). Modulos nao foram carregados.

### 3. Verificacao apos modprobe

```bash
ls /sys/class/dma/
lsmod | grep -i dma
```

**Resultado:** lista de canais ainda vazia. `lsmod` mostrou apenas `ezdma_fake` (o proprio driver do trabalho), sem nenhum provedor DMA.

---

## Diagnostico tecnico

O kernel `linux-azure` utilizado nas VMs Azure e **compilado de forma enxuta para producao cloud**: e otimizado para reduzir tamanho de imagem e tempo de boot, removendo modulos considerados desnecessarios para o ambiente virtualizado.

Os provedores de DMA Engine (`ioat`, `idxd`, `dmatest`, etc.) sao especificos para:

- **Hardware DMA fisico** (controladores Intel IDMA, Xilinx FPGAs, AMD ATA, etc.) — nao existem dentro da VM
- **`dmatest`** — provider de software para testes, NAO esta incluido no kernel `linux-azure`

O hipervisor Microsoft Hyper-V utilizado pela Azure **nao expoe controladores DMA fisicos** para dentro das VMs convidadas. Esta e uma decisao arquitetural padrao em hipervisores cloud: dispositivos DMA ficam abstraidos via paravirtualizacao.

---

## Tentativa adicional: instalar pacotes extras

Foi instalado o pacote `linux-modules-extra-$(uname -r)`, que adiciona drivers nao incluidos por padrao:

```bash
sudo apt-get install linux-modules-extra-$(uname -r) -y
ls /lib/modules/$(uname -r)/kernel/drivers/dma/
```

**Resultado:** 9 modulos disponiveis (`altera-msgdma`, `idma64`, `idxd`, `ioat`, `plx_dma`, `ptdma`, `sf-pdma`, `xilinx`, `dw`). **Todos sao drivers para hardware fisico especifico que nao existe na VM Azure**. Nenhum deles cria canal DMA usavel no ambiente.

O modulo `dmatest` (unico que cria canal puramente em software) **NAO esta incluido** no pacote `linux-modules-extra` para kernel `linux-azure`.

---

## Por que a implementacao atual (v3.0) e a maxima possivel no ambiente

A DMA Engine API divide-se em duas camadas:

1. **Camada DMA Coherent** (`dma_alloc_coherent`, `dma_mmap_coherent`, `dma_set_coherent_mask`)
   - Cuida de alocacao e mapeamento de memoria com cache coherency garantido
   - Funciona em qualquer ambiente Linux com sistema de memoria virtual
   - **IMPLEMENTADA no driver v3.0** com bus address de 64 bits comprovado (`bus=0x0000000102bfd000`)

2. **Camada DMA Async Transfer** (`dma_request_channel`, `dmaengine_prep_dma_memcpy`, `dma_async_issue_pending`)
   - Cuida da transferencia em si, orquestrada pelo controlador DMA fisico
   - Requer canal DMA exposto pelo hardware/hipervisor
   - **NAO E POSSIVEL implementar neste ambiente** por limitacao do hipervisor

