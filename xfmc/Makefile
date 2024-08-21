
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.

obj-m += hdmi21-xfmc.o

ccflags-y := -DBASE_BOARD_VEK280

# HDMI 2.1 FMC
hdmi21-xfmc-objs := x_vfmc.o
hdmi21-xfmc-objs += fmc.o
hdmi21-xfmc-objs += fmc74.o
hdmi21-xfmc-objs += fmc64.o
hdmi21-xfmc-objs += fmc65.o
hdmi21-xfmc-objs += tipower.o
hdmi21-xfmc-objs += idt.o
hdmi21-xfmc-objs += onsemi_tx.o
hdmi21-xfmc-objs += onsemi_rx.o
hdmi21-xfmc-objs += ti_tmds1204_tx.o
hdmi21-xfmc-objs += ti_tmds1204_rx.o

hdmi21-xfmc-objs += si5344.o
