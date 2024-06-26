# SPDX-License-Identifier: GPL-2.0-only
#
# Copyright (C) 2022 BAIKAL ELECTRONICS, JSC
#
# Baikal-T1 dtbs makefile
#
dtb-$(CONFIG_BT1_DTB_ALL) += bt1-gen.dtb bfk3.dtb bt1-mitx.dtb
dtb-$(CONFIG_BT1_DTB_GENERIC) += bt1-gen.dtb
dtb-$(CONFIG_BT1_DTB_BFK) += bfk3.dtb
dtb-$(CONFIG_BT1_DTB_MITX) += bt1-mitx.dtb

obj-$(CONFIG_BUILTIN_DTB) += $(addsuffix .o, $(dtb-y))
