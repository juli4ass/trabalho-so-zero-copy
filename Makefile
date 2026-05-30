obj-m += ezdma_fake.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: module benchmark

module:
	make -C $(KDIR) M=$(PWD) modules

benchmark: benchmark.c
	gcc -O2 -Wall -o benchmark benchmark.c

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f benchmark
