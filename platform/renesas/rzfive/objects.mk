#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2022 Renesas Electronics Corp.
#

# Compiler flags
platform-cppflags-y =
platform-cflags-y =
platform-asflags-y =
platform-ldflags-y =

# Objects to build
platform-objs-y += ../../andes/ae350/cache.o platform.o

# Blobs to build
FW_TEXT_START=0x00000000
FW_DYNAMIC=y
