# Copyright 2025 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later

FILESYSTEM_DRIVER ?=
ifneq ($(strip $(FILESYSTEM_DRIVER)),)

    # Filesystem Configurables
    VALID_FILESYSTEM_DRIVERS := lfs_flash lfs_eeprom

    ifeq ($(filter $(FILESYSTEM_DRIVER),$(VALID_FILESYSTEM_DRIVERS)),)
        $(call CATASTROPHIC_ERROR,Invalid FILESYSTEM_DRIVER,FILESYSTEM_DRIVER="$(FILESYSTEM_DRIVER)" is not a valid filesystem driver)
    else
        OPT_DEFS += -DFILESYSTEM_ENABLE
        COMMON_VPATH += \
            $(MODULE_PATH_FILESYSTEM)/filesystem
        # SRC += filesystem.c # This is already included in the build by virtue of having the same name as the module -- when promoting to core, need to uncomment this line

        # If we're using a littlefs driver, set up the common littlefs items
        ifeq ($(strip $(FILESYSTEM_DRIVER:lfs_%=lfs_)),lfs_)
            # Common littlefs stuff
            COMMON_VPATH += \
                $(MODULE_PATH_FILESYSTEM)/littlefs
            SRC += \
                lfs.c \
                lfs_util.c \
                fs_lfs_common.c
            OPT_DEFS += -DLFS_NO_MALLOC -DLFS_THREADSAFE -DLFS_NAME_MAX=40 -DLFS_NO_ASSERT
        endif

        ifeq ($(strip $(FILESYSTEM_DRIVER)),lfs_flash)
            SRC += fs_lfs_flash.c

            # replace with NVM_DRIVER = filesystem
            NVM_DRIVER := custom
            # work out how to set these correctly
            FLASH_DRIVER := spi
            SPI_DRIVER_REQUIRED = yes
            #EEPROM_DRIVER := none
            #WEAR_LEVELING_DRIVER := none
        endif

        ifeq ($(strip $(XAP_ENABLE)),yes)
            COMMON_VPATH += $(MODULE_PATH_FILESYSTEM)/nvm
            SRC += \
                nvm_eeconfig.c \
                nvm_dynamic_keymap.c
        else ifeq ($(strip $(VIA_ENABLE)),yes)
            COMMON_VPATH += $(MODULE_PATH_FILESYSTEM)/nvm
            SRC += \
                nvm_eeconfig.c \
                nvm_dynamic_keymap.c \
                nvm_via.c
        else ifeq ($(strip $(DYNAMIC_KEYMAP_ENABLE)),yes)
            COMMON_VPATH += $(MODULE_PATH_FILESYSTEM)/nvm
            SRC += \
                nvm_eeconfig.c \
                nvm_dynamic_keymap.c
        else
            COMMON_VPATH += $(MODULE_PATH_FILESYSTEM)/nvm
            SRC += \
                nvm_eeconfig.c
        endif
    endif

endif
