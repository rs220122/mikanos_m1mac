SHELL=/bin/bash

BUILD_DIR  = $(EDK2_DIR)/Build/

LOADER     = $(BUILD_DIR)/MikanLoaderX64/DEBUG_CLANGPDB/X64/Loader.efi
KERNEL     = kernel/kernel.elf

.PHONY: all
all: loader kernel

.PHONY: kernel
kernel:
	@make -s -C kernel

.PHONY: loader
loader:
	@make -s -C MikanLoaderPkg

.PHONY: clean
clean: clean-kernel clean-loader

.PHONY: clean-kernel
clean-kernel:
	@make -C kernel clean

.PHONY: clean-loader
clean-loader:
	@make -C MikanLoaderPkg clean

.PHONY: qemu
qemu: loader kernel
	@$(OSBOOK_DIR)/devenv/run_qemu.sh $(LOADER) $(KERNEL)