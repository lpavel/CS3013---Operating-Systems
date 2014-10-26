obj-m := hello.o module.o openclose.o intercept.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

	gcc test.c -o test
	gcc testnull.c -o testnull
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm test testnull