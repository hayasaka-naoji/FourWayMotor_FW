# FourWayMotor_FW — STM32CubeIDE 工程

根据 `FourWay_Motor_STM32.hex`（v1.6.5）逆向结果重建的四路电机驱动固件。
原固件由 Keil/ARMCC 编译、无符号；本工程用 **STM32CubeIDE + HAL** 重写，行为、引脚、
协议和参数布局与原机保持一致，但**不是逐字节可复现的同一份二进制**。
仓库不包含原厂固件及其反汇编；`re/` 只保留分析报告和脚本。

- MCU：**STM32F103RCTx**（LQFP64，256 KB Flash / 48 KB RAM，高密度）
- 时钟：HSE 8 MHz × 9 = **72 MHz**
- 控制环：**100 Hz**（TIM6），PWM **20 kHz**（TIM1 + TIM8）
- 串口：USART1 / USART2，**115200 8N1**
- I2C2 从机：7 位地址 **0x26**（OAR1 = 0x4C）
- 参数区：Flash `0x08024000`（2 KB 页模拟 EEPROM，魔数 `0xAA55`）

> 为什么选 F103RC：原固件用到 TIM5、TIM8 和 2 KB Flash 页，且参数页在 144 KB 偏移处，
> 只能是高密度型号（F103xC/D/E）。若你的板子是 F103VE/ZE，只需把工程属性里的 MCU
> 改成对应型号，并把链接脚本的 FLASH 长度改为 512K/768K。

---

## 1. 目录结构

```
FourWayMotor_FW/
├── .cproject / .project       STM32CubeIDE 工程文件（已配好）
├── STM32F103RCTX_FLASH.ld     256K/48K 链接脚本（含参数页保护区 ASSERT）
├── Makefile                   命令行构建（与 IDE 构建等价）
├── Core/
│   ├── Startup/startup_stm32f103xe.s
│   ├── Inc/
│   │   ├── app_config.h       引脚、参数偏移、限幅、默认值、可选第二颗 LED
│   │   ├── board.h / fmt.h / params.h / encoder.h / led.h
│   │   ├── motor.h / protocol.h / i2c_slave.h
│   │   ├── stm32f1xx_hal_conf.h
│   │   └── stm32f1xx_it.h
│   └── Src/
│       ├── main.c             初始化 + 主循环 + 100 Hz 回调
│       ├── board.c            时钟、ADC、UART、TIM6
│       ├── led.c              状态灯模式（启动/空闲/运行/写Flash/错误）
│       ├── encoder.c          4 路正交编码器
│       ├── motor.c            PWM + 增量式 PID
│       ├── params.c           Flash 参数读写
│       ├── protocol.c         ASCII 协议 + 遥测
│       ├── i2c_slave.c        I2C2 从机寄存器接口
│       ├── fmt.c              无 printf 的定点格式化/解析
│       ├── syscalls.c         newlib 桩（_write/_read/_sbrk…）
│       ├── stm32f1xx_it.c     中断向量实现
│       ├── stm32f1xx_hal_msp.c 时钟/GPIO/NVIC
│       └── system_stm32f1xx.c
├── Drivers/                   CMSIS + STM32F1xx HAL（来自 STM32Cube FW F1）
├── re/                        逆向分析报告与脚本（不含原厂固件）
└── build/                     CLI 构建输出（可删除）
```

---

## 2. 打开与编译

### 2.1 STM32CubeIDE

1. `File → Import → General → Existing Projects into Workspace`
2. 选择本目录 `FourWayMotor_FW`，Finish
3. 直接 Build（`Project → Build Project`）

工程已经配置好：
- MCU = STM32F103RCTx，Cortex-M3
- 宏：`USE_HAL_DRIVER`、`STM32F103xE`（Debug/Release 都有）
- 链接脚本：`STM32F103RCTX_FLASH.ld`
- 源目录：`Core`、`Core/Startup`、`Drivers`

### 2.2 设备头文件说明（重要）

工程里的 `Drivers/CMSIS/Device/ST/STM32F1xx/Include` 只带了 `stm32f103xb.h`（模板遗留），
高密度的 `stm32f103xe.h` 直接从本机 Cube 仓库引用：

```
C:/Users/watson/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7/Drivers/CMSIS/Device/ST/STM32F1xx/Include
```

该路径已写入 `.cproject` 的 include path。**换机器时**把这条路径改成你自己的 Cube 仓库路径，
或者把 `stm32f103xe.h` 复制到：

```
Drivers/CMSIS/Device/ST/STM32F1xx/Include/
```

复制后就可以删掉那条外部 include path，工程即完全自包含。

### 2.3 命令行

```bash
make            # 需要 arm-none-eabi-gcc 在 PATH 中
```

或指定 CubeIDE 自带工具链：

```bash
make PREFIX=/c/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344/tools/bin/arm-none-eabi-
```

产物：`build/FourWayMotor_FW.elf / .hex / .bin`。
当前构建大小：text ≈21.6 KB，data 12 B，bss 5.7 KB（与原固件 22.7 KB 量级相当）。
构建过程零警告；镜像结束于 `0x080054B0`，距离参数页 `0x08024000` 很远。

可以直接烧写的镜像：`build/FourWayMotor_FW.hex` 或 `build/FourWayMotor_FW.bin`（基址 0x08000000）。

> 已用 CubeIDE 1.19.0 的无头构建器（`org.eclipse.cdt.managedbuilder.core.headlessbuild`）
> 验证过本工程可以正常导入并生成 ELF；命令行构建用 CubeIDE 自带的 GCC 13.3 也通过。

### 2.4 关于 “4 errors” 的 newlib 警告

如果构建日志里出现：

```
warning: _close / _lseek / _read / _write is not implemented and will always fail
```

并显示“构建失败”，那是 CDT 错误解析器把 libnosys 的 4 条链接**警告**同时计成了错误
（此时 ELF 其实已经生成）。本工程通过 `Core/Src/syscalls.c` 提供了这些桩函数，
正常构建结果是 **0 错误 0 警告**。

如果 IDE 里仍然显示旧错误，做一次：Project Explorer 右键 → **Refresh (F5)** →
**Project → Clean** → **Build Project**（新增文件是在 IDE 外创建的，需要刷新资源）。

---

## 3. 引脚

| 功能 | 引脚 | 说明 |
|---|---|---|
| PWM 电机 0 | **PB0 / PB1** | TIM1_CH2N / CH3N（TIM1 部分重映射） |
| PWM 电机 1 | **PC6 / PC7** | TIM8_CH1 / CH2 |
| PWM 电机 2 | **PA8 / PA11** | TIM1_CH1 / CH4 |
| PWM 电机 3 | **PC8 / PC9** | TIM8_CH3 / CH4 |
| 编码器 0 | **PA0 / PA1** | TIM5_CH1 / CH2 |
| 编码器 1 | **PA6 / PA7** | TIM3_CH1 / CH2 |
| 编码器 2 | **PA15 / PB3** | TIM2_CH1 / CH2（TIM2 部分重映射 1，关闭 JTAG） |
| 编码器 3 | **PB6 / PB7** | TIM4_CH1 / CH2 |
| USART1 | **PA9 / PA10** | TX / RX |
| USART2 | **PA2 / PA3** | TX / RX |
| I2C2 | **PB10 / PB11** | SCL / SDA，开漏 |
| 电池检测 | **PC4** | ADC1_IN14，分压比 4.03 |
| 状态灯 | **PC13** | 见 §3.1 指示模式 |
| SWD | PA13 / PA14 | 保留调试 |

PWM 是 20 kHz、ARR = 3599。电机 0 和 2 走 TIM1 的互补（N）输出，逻辑上相当于反相驱动；
代码里的 CCR 写法已经按原固件处理（一路 0、另一路 |v|）。

### 3.1 状态灯

PC13 由 `led.c` 在 100 Hz 心跳中驱动，不再固定 3 s 双闪。原逆向报告曾把它误记为 PC5；
按位带别名 `0x422201B4` 反算得到 GPIOC ODR bit 13，且原固件 `GPIO_Init`（0x08002D94）
把 Pin 13 配置为 2 MHz 推挽输出，因此确定为 **PC13**。若你的板子 LED 是低电平点亮，
把 `Core/Inc/app_config.h` 里的 `LED_ACTIVE_HIGH` 改为 0。

| 模式 | 表现 | 触发条件 |
|---|---|---|
| 启动 | 3 次 100 ms 快闪 | 上电/复位后约 450 ms |
| 空闲 | 每 2 s 闪 50 ms | 没有运动指令 |
| 运行 | 每 1 s 双闪 | `$spd` 或 `$pwm` 有非零目标 |
| 写参数 | 常亮 | 擦写参数页的 20–40 ms |
| 错误 | 5 Hz 快闪 2 s | 命令超长 / Flash 写失败 |

“运行”指示的是**指令状态**，不是编码器实测速度；`$spd 0,0,0,0#` 或 `$pwm 0,0,0,0#`
会回到空闲模式。若板上有第二颗 LED，可在 `Core/Inc/app_config.h` 取消 `IND_LED_ENABLE`
注释并设置端口/引脚（支持 `IND_LED_ACTIVE_HIGH`），它会随运动指令点亮。

---

## 4. 控制算法

```
counts_per_rev = mline × mphase × 4
速度(mm/s) = 编码器增量 × 100 × (π × 轮径mm) / counts_per_rev

e[n] = 目标速度 - 实际速度
u[n] = u[n-1] + Kp·(e[n]-e[n-1]) + Ki·e[n] + Kd·(e[n]-2e[n-1]+e[n-2])
u = clamp(u, ±2000)
PWM = clamp(u ± deadzone, ±3600)
```

- `spd` 命令进入闭环；`pwm` 命令退出闭环直接给占空比
- `mtype == 4` 时控制环完全跳过编码器与 PID（开环型号）
- `mtype == 2` 时四路编码器计数方向取反（第 4 路相反）

默认参数（与原机一致）：mtype=1，deadzone=1600，mline=11，mphase=30，
轮径 67.0 mm，PID = 0.8 / 0.06 / 0.5。

---

## 5. 串口协议

帧格式：`$<命令名> <参数>#`，命令名大小写均可，多参数用 `, ` 分隔。

| 命令 | 参数 | 说明 | 响应 |
|---|---|---|---|
| `$mtype 1#` | 1–4 | 电机/编码器类型，4=开环 | `Set mtype:OK!` |
| `$flash_reset#` | – | 恢复出厂参数 | `falsh_reset:OK!`（原固件拼写） |
| `$read_flash#` | – | 打印全部参数 | `read_flash:OK!` + 参数行 |
| `$read_vol#` | – | 电池电压 | `$Battery:12.6V#` |
| `$deadzone 1600#` | 0–3600 | 死区补偿 | `Set deadzone:OK!` |
| `$mline 11#` | u16 | 编码器线数 | `Set mline:OK!` |
| `$mphase 30#` | u16 | 减速比 | `Set mphase:OK!` |
| `$wdiameter 67.0#` | >0 | 轮径 mm | `Set wdiameter:OK!` |
| `$mpid 0.8,0.06,0.5#` | 3 个 >0 | PID，**写完后 MCU 复位** | 无（先打印 `Reset MCU`） |
| `$upload 1,0,1#` | 3 个 0/1 | MAll / MTEP / MSPD 开关 | 无 |
| `$spd 300,-300,300,-300#` | ±1000 | 闭环目标速度 mm/s | 无 |
| `$pwm 500,500,500,500#` | ±3600 | 开环 PWM | 无 |

响应同时从 USART1 和 USART2 发出。版本信息只在 USART1 打印（注意原固件格式串带前导空格）：

```
 Motor_Version:1.6.5
```

遥测（`upload` 打开后）：

```
$MAll:%d,%d,%d,%d#     累计编码器位置
$MTEP:%d,%d,%d,%d#     每 10 ms 编码器增量
$MSPD:%.2f,%.2f,%.2f,%.2f#   实际速度 mm/s（50 ms 节流）
```

---

## 6. I2C 从机

7 位地址 0x26。写事务：`[cmd][data_hi][data_lo]`；读事务：先写 1 字节寄存器号，再读 2 字节（高字节在前）。

写命令：

| cmd | 含义 | 数据 |
|---|---|---|
| 0 | mtype | 2 字节，1–4 |
| 1 | deadzone | 2 字节，0–3600 |
| 2 | mline | 2 字节 |
| 3 | mphase | 2 字节 |
| 4 | 轮径 | ASCII，如 `"67.0"` |
| 5 | 目标速度 | 4 × int16 大端 |
| 6 | 直接 PWM | 4 × int16 大端 |

读寄存器：

| 寄存器 | 内容 |
|---|---|
| 0x08 | 电池电压 ×10 |
| 0x10–0x13 | 4 路增量 |
| 0x20/0x21 | 位置 0 高/低 16 位 |
| 0x22/0x23 | 位置 1 |
| 0x24/0x25 | 位置 2 |
| 0x26/0x27 | 位置 3 |

> I2C 从机的寄存器级实现已经按上述语义写好，但**没有在真实总线上验证过**，
> 建议第一次上板时用逻辑分析仪确认重复起始和 STOP 时序。
> 中断处理里已修正“ADDR 后仍用旧 SR1 快照判断 TXE”的问题（避免一次中断内重复写 DR），
> 这条路径仍需实测确认。

---

## 7. 参数区

Flash 页 `0x08024000`，魔数 `0xAA55` 在 +0x02。字段偏移与原固件完全相同：

| 偏移（相对 0x08024000） | 类型 | 参数 |
|---|---|---|
| +0x10 | u16 | mtype |
| +0x14 | u16 | deadzone |
| +0x20 | u16 | mline |
| +0x30 | u16 | mphase |
| +0x50 | float | 轮径 |
| +0x80 | float | Kp |
| +0x86 | float | Ki |
| +0x92 | float | Kd |

写入方式：整页读入 RAM → 修改 → 擦除 → 回写；若目标值与 Flash 中完全一致则跳过擦写
（避免无谓磨损和 20–40 ms 停顿），无磨损均衡。
擦写期间会关中断约 20–40 ms，串口可能丢字节，属正常现象。

链接脚本已加 ASSERT：应用镜像不得越过 `0x08024000`，防止代码把参数页写坏。

---

## 8. 与原固件的差异（有意为之）

| 项 | 原固件 | 本工程 | 原因 |
|---|---|---|---|
| 工具链 | Keil ARMCC | GCC + HAL | CubeIDE 生态 |
| SysTick | 未使用 | 1 ms（HAL 时基） | HAL 超时/延时需要 |
| `$mpid` 校验 | 三个增益必须 >0 | 相同 | 保持兼容 |
| 命令缺空格 | 长度下溢，可能越界写 RAM | 已加边界检查 | 修复原固件缺陷 |
| 参数写入 | 每次整页擦写 | 值未变时跳过 | 减少 Flash 磨损与中断停顿 |
| 状态灯 | 3 s 周期双闪 | 启动/空闲/运行/写Flash/错误 指示 | 便于现场判断状态 |
| 整数解析 | 无溢出检查 | 溢出返回错误 | 修复未定义行为 |
| 响应前导空格 | `" Set ...:OK!"`、`" $MAll:..."` 等带一个前导空格 | 不带 | 现有文档/上位机按无空格处理 |
| MPID 复位 | 是（`Reset MCU`） | 相同 | 保持兼容 |

原固件里 `Set MPID:OK!` 是**不可达代码**（写完立即复位），本工程同样不会发这个回包，
上位机不要等它。

---

## 9. 上板验证清单

由于没有目标板，以下项目需要实测确认：

1. **PWM 方向**：`$pwm 300,0,0,0#` 看电机 0 转向是否符合预期；若反了，交换该路两个 CCR。
2. **编码器方向**：手转轮子，`$upload 1,0,0#` 看 `$MAll` 是否随正转变大；方向反了改 `mtype`
   或交换 A/B 相。
3. **电流/死区**：默认 deadzone=1600 是按原机参数给的；不同电机需要重新标定。
4. **I2C**：确认地址 0x26、重复起始读时序。
5. **电池分压**：用万用表对比 `$read_vol#`，必要时改 `BATT_DIVIDER`。
6. **闭环稳定性**：先小目标速度（如 `$spd 100,100,100,100#`），再逐步加大。
7. **状态灯**：上电应看到 3 次快闪；发 `$spd 100,100,100,100#` 后应变为每秒双闪；
   发 `$spd 0,0,0,0#` 回到每 2 s 单闪。

---

## 10. 可选的 CubeMX 使用方式

本工程**没有** `.ioc`，所有外设都是手写初始化，避免 CubeMX 重新生成时覆盖应用代码。
如果你想用 CubeMX 继续加外设，可以新建一个 `.ioc` 并把本目录的 `Core/Src` 排除在生成范围外，
或者只把 CubeMX 用来核对引脚，不要让它覆盖 `board.c / motor.c / encoder.c`。
