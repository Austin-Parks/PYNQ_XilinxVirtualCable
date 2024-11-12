#export CROSS_COMPILE := arm-linux-gnu-
export ARCH := arm
MYCC := $(CROSS_COMPILE)gcc

DRIVER := xilinx_xvc_driver
XVCSERVER := xvcServer
TEST := verify_$(DRIVER)
IOCTL_HDR_DIR := ../driver

.PHONY: mmap
mmap: $(XVCSERVER)

.PHONY: all
all: mmap

$(XVCSERVER): $(XVCSERVER).c
	$(MYCC) -Wall -o $@ $^

.PHONY: clean
clean:
	rm -f $(XVCSERVER)
