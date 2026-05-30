# Driver Zero-Copy I/O - ezdma

Trabalho da disciplina Sistemas Operacionais - FURB / Sistemas de Informacao
Autora: Julia da Assuncao Silva e grupo

## Tema
Otimizacao de I/O em Sistemas de Tempo Real:
Implementacao de Zero-Copy e DMA.

## Conteudo
- ezdma_fake.c - Driver de Kernel Linux (LKM) com read() + mmap()
- Makefile - Compilacao out-of-tree
- benchmark.c - Comparativo read() vs mmap()

## Como rodar
sudo apt-get install build-essential linux-headers-$(uname -r) -y
make
sudo insmod ezdma_fake.ko
./benchmark
sudo rmmod ezdma_fake

## Resultados (Ubuntu 22.04 / Azure Standard_D2s_v3)

| Execucao | read (ns/op) | mmap (ns/op) | Speedup |
|---:|---:|---:|---:|
| 1 | 2042 | 204 | 9.99x |
| 2 | 2052 | 88 | 23.12x |
| 3 | 2055 | 117 | 17.54x |
| 4 | 4123 | 168 | 24.53x |
| Media | ~2500 | ~145 | ~18x |

mmap() (Zero-Copy) ~18x mais rapido que read() (tradicional).

## Ambiente
- Ubuntu 22.04 LTS
- Kernel: 6.8.0-1052-azure
- VM: Azure Standard_D2s_v3 (Standard Security)
