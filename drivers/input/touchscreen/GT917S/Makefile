# Linux driver folder
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gt9xx_mtk/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/gt9xx_mtk/include/$(CONFIG_GTP_CONFIG)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

obj-y	+=  gt9xx_driver.o
obj-y	+=  gt9xx_extents.o
obj-y	+=  gt9xx_update.o
obj-y	+=  goodix_tool.o
obj-y   +=  gt9xx_tpd_control.o
obj-y   +=  gt9xx_tpd_button.o
obj-y   +=  gt9xx_tpd_debug.o
obj-y   +=  gt9xx_tpd_default.o
obj-y   +=  gt9xx_tpd_misc.o
obj-y   +=  gt9xx_tpd_setting.o

