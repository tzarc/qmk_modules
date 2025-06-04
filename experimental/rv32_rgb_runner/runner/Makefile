PROJECT := rv32_runner
TARGETS := $(PROJECT).elf $(PROJECT).bin $(PROJECT).debug.txt
OBJ_DIR := .build

all : $(TARGETS)
size: all
	@echo
	$(PREFIX)size --radix=16 $(PROJECT).elf
	@echo
	$(PREFIX)size --radix=10 $(PROJECT).elf

PREFIX := riscv32-unknown-elf-
CFLAGS := -fno-stack-protector -fno-common -flto=auto
CFLAGS += -static-libgcc -fdata-sections -ffunction-sections
CFLAGS += -g -Os -march=rv32ima_zicsr -mabi=ilp32 -static
CXXFLAGS += -fno-rtti -fno-exceptions
LDFLAGS := -T flatfile.lds -nostdlib -Wl,--gc-sections

OBJS := $(wildcard *.c) $(wildcard *.cpp) $(wildcard *.S)
OBJS := $(patsubst %.c,%.c.o,$(OBJS))
OBJS := $(patsubst %.cpp,%.cpp.o,$(OBJS))
OBJS := $(patsubst %.S,%.S.o,$(OBJS))
OBJS := $(addprefix $(OBJ_DIR)/, $(OBJS))

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJS): | $(OBJ_DIR)

$(OBJ_DIR)/%.c.o : %.c
	$(PREFIX)gcc -c -o $@ $^ $(CFLAGS)

$(OBJ_DIR)/%.S.o : %.S
	$(PREFIX)gcc -c -o $@ $^ $(CFLAGS)

$(OBJ_DIR)/%.cpp.o : %.cpp
	$(PREFIX)g++ -c -o $@ $^ $(CFLAGS) $(CXXFLAGS)

$(PROJECT).elf : $(OBJS)
	$(PREFIX)gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(PROJECT).debug.txt : $(PROJECT).elf
	$(PREFIX)objdump -wt $^ > $@
	$(PREFIX)objdump -wdS $^ >> $@

$(PROJECT).bin : $(PROJECT).elf
	$(PREFIX)objcopy $^ -O binary $@

clean:
	rm -rf $(TARGETS) $(OBJ_DIR)
