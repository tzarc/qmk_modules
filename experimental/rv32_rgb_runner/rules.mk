COMMON_VPATH += $(MODULE_PATH_RV32_RGB_RUNNER)/superior
SRC += rgb_matrix_rv32_runner.c

generated-files: $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h:
	@$(MAKE) -C $(MODULE_PATH_RV32_RGB_RUNNER)/inferior
	@{ echo 'static const unsigned char rv32_runner_bin[] = {'; \
		od -An -v -tx1 $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/rv32_runner.bin | sed -e 's@\([0-9a-f][0-9a-f]\)@0x\1,@g'; \
		echo '};'; \
		echo "static const unsigned int rv32_runner_bin_len = $$(wc -c <$(MODULE_PATH_RV32_RGB_RUNNER)/inferior/rv32_runner.bin | tr -d ' ');"; \
	} >$@

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.h)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/internal/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/internal/*.h)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/internal/*.S.in)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/internal/*.lds)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/common/api_bindings.h
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/rules.mk
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/Makefile
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/lib/mini-rv32ima/mini-rv32ima/mini-rv32ima.h
