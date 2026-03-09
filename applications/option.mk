##################################################################################
#	Copyright (c) PanGu Tech. Co., Ltd. 2022-2025. All rights reserved.
#	Desc:		Makefile 平台配置文件（示例）
#	FileName:	option.mk
#	Author:		huangdajiang
#	Date:		2022-08-24
##################################################################################

MKFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MKFILE_DIR  := $(dir $(MKFILE_PATH))
include $(MKFILE_DIR)/config.mk

ifeq ($(VENDOR), UBUNTU)
CCOMPILE = gcc
CPPCOMPILE = g++
AR = ar
else ifeq ($(VENDOR), HI3516DV300)
CCOMPILE = arm-himix200-linux-gcc
CPPCOMPILE = arm-himix200-linux-g++
AR = arm-himix200-linux-ar
else ifeq ($(VENDOR), RK3576)
CCOMPILE = /home/nlj/workspace1/P001_rk3576_HM3X/host/bin/aarch64-buildroot-linux-gnu-gcc
CPPCOMPILE = /home/nlj/workspace1/P001_rk3576_HM3X/host/bin/aarch64-buildroot-linux-gnu-g++
AR = /home/nlj/workspace1/P001_rk3576_HM3X/host/bin/aarch64-buildroot-linux-gnu-ar
STRIP := /home/nlj/workspace1/P001_rk3576_HM3X/host/bin/aarch64-buildroot-linux-gnu-strip
endif

export CCOMPILE CPPCOMPILE AR
