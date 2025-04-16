# Copyright 2025 Nick Brassel (@tzarc)
# SPDX-License-Identifier: GPL-2.0-or-later

# Deal with macOS
ifeq ($(shell uname -s),Darwin)
SED = gsed
ECHO = gecho
else
SED = sed
ECHO = echo
endif

format:
	@git ls-files | grep -E '\.(c|h|cpp|hpp|cxx|hxx|inc|inl)$$' | grep -vE '\.q[gf]f\.' | grep -vE '(ch|hal|mcu)conf\.h$$' | grep -vE 'board.[ch]$$' | grep -vE '.inl.h$$' | while read file ; do \
		$(ECHO) -e "\e[38;5;14mFormatting: $$file\e[0m" ; \
		clang-format -i "$$file" ; \
	done
	@git ls-files | grep -E '(qmk_module|keyboard|keymap)\.json' | while read file ; do \
		$(ECHO) -e "\e[38;5;14mFormatting: $$file\e[0m" ; \
		qmk format-json -i "$$file" ; \
	done
