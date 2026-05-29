SRC += rgb_matrix_rv32_runner.c

generated-files: $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h:
	@$(MAKE) -C $(MODULE_PATH_RV32_RGB_RUNNER)/inferior
	[ ! -f $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h ] || rm -f $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h
	@cd $(MODULE_PATH_RV32_RGB_RUNNER)/inferior && \
		xxd -i rv32_runner.bin $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h
	sed -i 's@unsigned@static const unsigned@g' $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.h)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/*.lds)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/rules.mk
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/inferior/Makefile
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/lib/mini-rv32ima/mini-rv32ima/mini-rv32ima.h
