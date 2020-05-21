obj-m += add_dma_drv.o
KERNELPATH=/home/sunip/xilinx_linux/linux

all:
	make -C $(KERNELPATH) M=$(PWD) modules

clean:
	make -C $(KERNELPATH) M=$(PWD) clean