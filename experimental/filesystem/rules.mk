FILESYSTEM_DRIVER ?=
ifneq ($(strip $(FILESYSTEM_DRIVER)),)

    # Filesystem Configurables
    VALID_FILESYSTEM_DRIVERS := lfs_spi_flash

    ifeq ($(filter $(FILESYSTEM_DRIVER),$(VALID_FILESYSTEM_DRIVERS)),)
        $(call CATASTROPHIC_ERROR,Invalid FILESYSTEM_DRIVER,FILESYSTEM_DRIVER="$(FILESYSTEM_DRIVER)" is not a valid filesystem driver)
    else
        OPT_DEFS += -DFILESYSTEM_ENABLE
        COMMON_VPATH += \
            $(CURRENT_MODULE_PATH)/filesystem

        ifeq ($(strip $(FILESYSTEM_DRIVER)),lfs_spi_flash)
            COMMON_VPATH += \
                $(CURRENT_MODULE_PATH)/nvm \
                $(CURRENT_MODULE_PATH)/littlefs
            NVM_DRIVER := custom
            FLASH_DRIVER = spi
            SPI_DRIVER_REQUIRED = yes
            SRC += \
                lfs.c \
                lfs_util.c \
                fs_lfs_common.c \
                fs_lfs_spi_flash.c
            OPT_DEFS += -DLFS_NO_MALLOC -DLFS_THREADSAFE -DLFS_NAME_MAX=40 -DLFS_NO_ASSERT
        endif

        ifeq ($(strip $(XAP_ENABLE)),yes)
            SRC += \
                nvm_dynamic_keymap.c
        else ifeq ($(strip $(VIA_ENABLE)),yes)
            SRC += \
                nvm_dynamic_keymap.c \
                nvm_via.c
        else ifeq ($(strip $(DYNAMIC_KEYMAP_ENABLE)),yes)
            SRC += \
                nvm_dynamic_keymap.c
        endif
    endif

endif
