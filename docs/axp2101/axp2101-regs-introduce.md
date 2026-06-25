<h1 align="center"><font color=blue>AXP2101常用寄存器简介</font></h1>

> Author : AW1742
>
> Date:2021-12-20



# 概述

## 编写目的

​		简要介绍PMU AXP2101芯片的常用寄存器的使用说明及功能概述。

## 使用范围

​		Allwinner所有搭配axp2101的平台。

## 寄存器具体bit位介绍

​		后面的内容仅介绍寄存器的功能说明，涉及到具体的寄存器的某一个bit的功能说明，请查询PMU AXP2101的数据手册，在一号通上搜索axp2101关键字查找axp2101的芯片数据手册，也可直接点击此连接：https://one.allwinnertech.com/#/showDocPdf



# 1. comm_status & comm_cfg

## 1.1 comm_status

| reg  | Tpye | Default value | Reset Type | Description                                                  |
| ---- | ---- | ------------- | ---------- | ------------------------------------------------------------ |
| 0x00 | RD   | 0x0           | POR        | 描述VBUS负载情况、BATFET、battery、芯片温度、电流等的状态信息 |
| 0x01 | RD   | 0x0           | POR        | 描述电池的充电状态、VINDPM                                   |



## 1.2 comm_cfg

​		主要涉及BATFET、Battery、die_temp、vsys_min、vimdpm、iin_lim、PMU关机和复位等操作。

| reg  | Tpye | Default value | Reset Type       | Description                                 |
| ---- | ---- | ------------- | ---------------- | ------------------------------------------- |
| 0x10 | RW   | 0x30          | system reset/POR | PMU开关机/复位、PWRON、POWOK、VREF          |
| 0x12 | RW   | (EFUSE)       | POR              | BATFFET的使能控制及OCP关断使能              |
| 0x13 | RW   | 0x03          | POR              | DIE Temperaure的保护设置及level config      |
| 0x14 | RW   | 0x65          | POR              | 线性充电和开关充电电压的限制                |
| 0x15 | RW   | 0x06          | POR              | VINDPM：VBUS输入电压限制                    |
| 0x16 | RW   | 0x01          | POR              | 输入限流功能，总输入电流                    |
| 0x17 | RW   | 0x0           | POR              | gague相关复位操作                           |
| 0x18 | RW   | 0x0A          | POR              | gague、watchdong、button/cell battery使能位 |



# 2 开/关机源触发及设置

​		主要涉及开机源、开机源的获取及设置，还有唤醒源的设置。

| reg       | Tpye | Default value | Reset Type | Description                                       |
| --------- | ---- | ------------- | ---------- | ------------------------------------------------- |
| 0x20      | RD   | 0x0           | -          | 开机源                                            |
| 0x21      | RD   | 0x0           | POR        | 关机源                                            |
| 0x22      | RW   | (EFUSE)       | POR        | LDO、DIE -TEM、POWON的PWROFF_EN                   |
| 0x23      | RW   | 0x3f          | POR        | DCDC过压关机，dcdc1~dcdc5的欠压关机使能控制       |
| 0x24      | RW   | (EFUSE)       | POR        | 电池低电量关机设置                                |
| 0x26      | RW   | 0x08          | POR        | 设置PMU唤醒源，sleep_en、DCDC唤醒源保持休眠前的值 |
| 0x27      | RW   |               | POR        | 设置按键的短按中断、关机、开机的按键时间          |
| 0x28~0x2B |      |               |            | Fast pwron setting and control                    |



# 3 IRQ status & control

​		主要涉及PMU所有中断的使能及发生中断的标志位查看及清楚中断标志位。

| reg  | Tpye | Default value | Reset Type       | Description                                       |
| ---- | ---- | ------------- | ---------------- | ------------------------------------------------- |
| 0x40 | RW   | 0xff          | system reset     | Gague、battery irq_en                             |
| 0x41 | RW   | 0xfc          | system reset     | VBUS/Battery inset/remove PWRON IRQ_EN            |
| 0x42 | RW   | 0x5f          | system reset     | 电池充电完成、开始充电、DIE-TEM的一些中断使能设置 |
| 0x48 | RW1C | 0x00          | system reset/POR | 查看对应0x00的中断发生及清除中断标志位            |
| 0x49 | RW1C | 0x00          | system reset/POR | 查看对应0x01的中断发生及清除中断标志位            |
| 0x4A | RW1C | 0x00          | system reset/POR | 查看对应0x02的中断发生及清除中断标志位            |



# 4 TS设置

​		此处直接查看AXP2101的数据手册进行相关的设计即可，此处不详细列举。



# 5 Chaerge

​		主要涉及充电部分的设置，比如：预充电电流、充电电流、预充电安全时间、电充完成安全时间、纽扣充电电压设置、电池充电电压设置等。

| reg  | Tpye | Default value | Reset Type | Description                      |
| ---- | ---- | ------------- | ---------- | -------------------------------- |
| 0x61 | RW   | 0x05          | POR        | 预充电电流设置，默认为125mA      |
| 0x62 | RW   | (EFUSE)       | POR        | 充电电流设置                     |
| 0x64 | RW   | 0x03          | POR        | 电池充满限制电压                 |
| 0x67 | RW   | 0xd6          | POR        | 预充电和充电安全时间设置         |
| 0x6A | RW   | 0x03          | POR        | 纽扣电池充电电压限制，默认为2.9V |



# 6 DCDC 电源

​		主要涉及DCDC使能及电压设置。

| reg       | Tpye | Default value | Reset Type        | Description                                                  |
| --------- | ---- | ------------- | ----------------- | ------------------------------------------------------------ |
| 0x80      | RW   | EFUSE         | system reset      | DCDC1~DCDC5的使能开关                                        |
| 0x81      | RW   | 0x00          | system reset /POR | DCDC1~DCDC4d  PWM/PFM mode control,DCDC UVP debounce time config |
| 0x82~0x86 | RW   | EFUES         | system reset      | DCDC1~DCDC5的电压设置                                        |
| 0x87      | RW   | EFUSE         | POR               | DCDC1~DCDC3的最大输出电流设置                                |



# 7 LDO电源

​		主要涉及LDO电源的使能及电压设置。

| reg       | Tpye | Default value | Reset Type   | Description                                          |
| --------- | ---- | ------------- | ------------ | ---------------------------------------------------- |
| 0x90      | RW   | EFUSE         | system reset | aldo1~aldo4,bldo1~bldo2,cpusldo,dldo1 enable control |
| 0x91      | RW   | EFUSE         | system reset | dldo1 enable control                                 |
| 0x92~0x0A | RW   | EFUSE         | system reset | LDO电压设置                                          |

# 8 Gauge

| reg  | Tpye | Default value | Reset Type | Description                 |
| ---- | ---- | ------------- | ---------- | --------------------------- |
| 0xA1 | RW   | -             | POR        | 电池参数设置                |
| 0xA2 | RW   | 0x00          | POR        | 选择ROM或者sram存储配置参数 |
| 0xA4 | RD   | 0x00          | POR        | Battery percentage data     |

