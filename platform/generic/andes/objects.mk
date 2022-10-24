#
# SPDX-License-Identifier: BSD-2-Clause
#

carray-platform_override_modules-$(CONFIG_PLATFORM_ANDES_AE350) += andes_ae350
platform-objs-$(CONFIG_PLATFORM_ANDES_AE350) += andes/ae350.o

FW_TEXT_START=0x0
