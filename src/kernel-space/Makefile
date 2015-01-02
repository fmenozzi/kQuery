obj-m += kquery_mod.o

K=$(shell uname -r)
M=$(PWD)

all:
	make -C /lib/modules/$(K)/build M=$(M) modules

inst:
	make -C /lib/modules/$(K)/build M=$(M) modules_install

clean:
	make -C /lib/modules/$(K)/build M=$(M) clean

