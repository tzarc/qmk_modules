SRC += rgb_matrix_rv32_runner.c

generated-files: $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h:
	@$(MAKE) -C $(MODULE_PATH_RV32_RGB_RUNNER)/runner
	[ ! -f $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h ] || rm -f $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h
	@cd $(MODULE_PATH_RV32_RGB_RUNNER)/runner && \
		xxd -i rv32_runner.bin $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h
	sed -i 's@unsigned@static const unsigned@g' $(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h

$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/runner/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/runner/*.c)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/runner/*.h)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(wildcard $(MODULE_PATH_RV32_RGB_RUNNER)/runner/*.lds)
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/rules.mk
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/runner/Makefile
$(MODULE_PATH_RV32_RGB_RUNNER)/rv32_rgb_runner.inl.h: $(MODULE_PATH_RV32_RGB_RUNNER)/lib/mini-rv32ima/mini-rv32ima/mini-rv32ima.h
