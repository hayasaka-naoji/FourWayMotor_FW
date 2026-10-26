# FourWay_Motor_STM32.hex 逆向分析报告

> 目标：`D:\下载\FourWay_Motor_STM32.hex`（Intel HEX，64 025 字节文本 → 22 744 字节固件；该文件不随本仓库分发）
> 结论：**可以完整逆向**。无加密、无混淆、符号虽被剥离但结构清晰（Keil/ARMCC 编译的裸机固件）。
> 版本：**1.6.5**（固件内字符串 `Motor_Version:1.6.5`）

---

## 1. 固件身份

| 项 | 结论 |
|---|---|
| 架构 | ARM Cortex-M，Thumb-2，纯 Thumb |
| 目标芯片 | STM32F1 系列 **高密度型号**（F103xC/D/E 类；有 TIM5/TIM8，Flash 页 2 KB）或兼容芯片 |
| 编译器 | Keil ARM Compiler 5（ARMCC），带 C 库（printf/sprintf/atof）与软浮点（`__aeabi_f*`/`__aeabi_d*`） |
| 代码区 | `0x08000000 – 0x080058D8`（22.2 KB），单一连续段 |
| 系统时钟 | HSE 8 MHz × PLL9 = **72 MHz**（AHB 72 / APB2 72 / APB1 36，Flash latency 2 + prefetch） |
| 控制周期 | **100 Hz**（TIM6 更新中断，10 ms） |
| PWM | **20 kHz**（TIM1 + TIM8，ARR = 3599，PSC = 0） |
| 串口 | USART1、USART2 均为 **115200 8N1**，两条口内容完全对等（响应同时发两条） |
| 其他总线 | I2C2 **从机** 100 kHz，OAR1 = 0x4C（7 位地址 0x26） |
| 参数存储 | Flash 页 `0x08024000` 模拟 EEPROM，魔数 `0xAA55` |
| RAM | RW 0x20000000 + 0xAC，ZI 到 0x20001178，初始 SP = **0x20001178** |

`0x08024000` 的参数页说明这颗芯片至少有 144 KB Flash；结合 TIM8/TIM5 与 2 KB 页，基本可判定是 F103 高密度（如 F103RC/VC/ZE）或其国产兼容型号。

---

## 2. 内存映像与启动

```
0x08000000  +0x130  向量表（76 项 = 16 系统 + 60 IRQ）
0x08000130  startup：ldr sp,=0x20001178 ; bl 0x08000B34(scatterload) ; bx __main
0x08000148  Reset_Handler 垫片：bl SystemInit ; bx 0x08000130
0x08000162  Default_Handler（b .）
0x08001234  board_init()       ← 所有外设初始化
0x0800142C  cmd_parse()        ← ASCII 命令解释器
0x08001E24  uart_rx_byte()     ← 串口收帧状态机
0x08002570  flash_set_defaults()
0x08002640  flash_write_page()
0x08002EB8  speed_calc()
0x0800330C  pid_step()
0x08003A6C  TIM6_IRQHandler()  ← 100 Hz 控制环
0x0800422C  USART1_IRQHandler
0x08004344  USART2_IRQHandler
0x08005118  main()
0x08005218  i2c_read_reg()
0x080055A8  telemetry()
0x08005831  "1.6.5"
```

中断使用情况：

| 向量 | 处理 | 说明 |
|---|---|---|
| Reset | 0x08000130/0x08000148 | 正常启动 |
| NMI / SVC / PendSV | `bx lr` 空返回 | 没有 RTOS |
| SysTick | `b .` | 未使用，延时/节拍靠 TIM6 |
| HardFault/MemManage/BusFault/UsageFault | `nop; b .` | 死循环，无故障记录 |
| IRQ33/34 I2C2_EV/ER | 0x08002A5C / 0x08002A1C | I2C 从机 |
| IRQ37 USART1 | 0x0800422C | 收字节 → 0x08001E24 |
| IRQ38 USART2 | 0x08004344 | 同上 |
| IRQ54 TIM6 | 0x08003A6C | 100 Hz 控制环 |
| 其余 56 个 IRQ | Default_Handler | 未用 |

---

## 3. 外设与引脚

### 3.1 PWM（4 路 H 桥，每路 2 个半桥）

PWM 初始化：`PWM_Init(period=0xE10, psc=0)` @ 0x08003088
- TIM1：**部分重映射**（`AFIO_MAPR` 写 `GPIO_PartialRemap_TIM1 = 0x00160040`）
- TIM8：无重映射
- 两定时器同步 20 kHz

引脚（由 0x08002DC0 的 8 项 AF_PP 表 + 重映射推出）：

| 定时器/通道 | 引脚 | 用于 |
|---|---|---|
| TIM1_CH1 | PA8 | 电机 2 半桥 A |
| TIM1_CH2N | PB0 | 电机 0 半桥 A |
| TIM1_CH3N | PB1 | 电机 0 半桥 B |
| TIM1_CH4 | PA11 | 电机 2 半桥 B |
| TIM8_CH1 | PC6 | 电机 1 半桥 A |
| TIM8_CH2 | PC7 | 电机 1 半桥 B |
| TIM8_CH3 | PC8 | 电机 3 半桥 A |
| TIM8_CH4 | PC9 | 电机 3 半桥 B |

电机编号 → CCR 对（来自 0x08003184）：

```
motor0 : TIM1 CCR2 / CCR3   （实际输出走 CH2N/CH3N = PB0/PB1，反相）
motor1 : TIM8 CCR1 / CCR2   （PC6 / PC7）
motor2 : TIM1 CCR1 / CCR4   （PA8 / PA11）
motor3 : TIM8 CCR3 / CCR4   （PC8 / PC9）
```

驱动方式：每路两半桥一个 CCR=0、另一个 CCR=|v|，属于差分/锁相驱动；TIM1 那两路因为走的是 N 输出，逻辑上反相，代码里已按反相写法处理。

### 3.2 编码器（4 路，硬件正交解码）

| 逻辑编号 | 定时器 | 引脚 | 重映射 |
|---|---|---|---|
| 0 | TIM5 | PA0 / PA1 | 无 |
| 1 | TIM3 | PA6 / PA7 | 无 |
| 2 | TIM2 | PA15 / PB3 | TIM2 部分重映射 1 |
| 3 | TIM4 | PB6 / PB7 | 无 |

- 编码器模式 3（A/B 双边沿），ARR = 0xFFFF，计数器初值 **0x7FFF**（中点）
- 每 10 ms 读一次：`delta = 0x7FFF - CNT`，随后把 CNT 重置为 0x7FFF（0x08002290）

### 3.3 其他

| 功能 | 引脚/资源 |
|---|---|
| USART1 TX/RX | PA9 / PA10 |
| USART2 TX/RX | PA2 / PA3 |
| I2C2 SCL/SDA | PB10 / PB11（开漏复用） |
| 电池电压 | ADC1_IN14 = **PC4**，换算 `V = ADC × 3.3/4096 × 4.03`（约 4.03 分压比） |
| 工作灯 | **PC13**（GPIOC ODR bit 13），通过位带别名 0x422201B4 操作；`GPIO_Init` 配置为 2 MHz 推挽输出（0x08002D94）；3 s 周期内闪两次（每次 300 ms）。早期版本误记为 PC5 |
| JTAG | 关闭 JTAG、保留 SWD（`GPIO_Remap_SWJ_JTAGDisable`） |

---

## 4. 运行时结构

没有 RTOS。主循环 + 一个 100 Hz 定时中断：

```
main():
    board_init()
    printf("Motor_Version:%s\r\n", "1.6.5")
    for (;;) {
        if (g_cmd_state == 2) {              // 帧超长
            g_cmd_state = 0;
            strcpy(resp, "fail,command is long!\r\n");
            uart1_send(resp); uart2_send(resp);
        } else if (g_cmd_state == 1) {       // 收到完整命令
            g_cmd_state = 0;
            cmd_parse();                     // 解释并执行 + 回包
        }
        telemetry();                         // 按 upload 开关输出遥测
    }
```

串口接收状态机（0x08001E24）：
- 空闲时遇到 `$` → 清缓冲、进入接收
- 收到 `#` → 复制 50 字节命令体、置“命令就绪”
- 长度 > 50 → 置“超长”错误

TIM6 中断（0x08003A6C，每 10 ms）：

```
if (TIM6 update) {
    clear flag;
    if (mtype != 4) {
        encoder_accumulate();    // 读 4 路编码器，累加位置
        speed_pid_output();      // 计算速度 → PID → PWM
    }
    if (--bat_tick == 0) { bat_tick = 20; battery = adc_read()*3.3/4096*4.03; }  // 200 ms
    if (--mspd_tick == 0) { mspd_tick = 5; mspd_flag = 1; }                      // 50 ms
    led_heartbeat();
}
```

---

## 5. 电机控制链路（核心算法）

### 5.1 编码器 → 速度

```
counts_per_rev = mline * mphase * 4            // mline×减速比×4（四倍频）
circumference  = π * wdiameter                 // mm
speed_i [mm/s] = (pos_i - pos_prev_i) * 100 * circumference / counts_per_rev
```

`×100` 是把“每 10 ms 的计数”换算成“每秒”。所以 **spd 命令的单位是 mm/s**，不是 m/s，也不是 RPM。

默认 `mline=11, mphase=30` → 1320 counts/rev，正好是常见 TT 电机（11 线编码器 + 30:1 减速箱）的配置；`wdiameter=67.0 mm`。

方向约定（0x0800230C）：
```
if (mtype == 2) { pos0+=d0; pos1+=d1; pos2+=d2; pos3-=d3; }
else            { pos0-=d0; pos1-=d1; pos2-=d2; pos3+=d3; }
```

### 5.2 增量式 PID（0x0800330C）

```
e[n] = target - actual
u[n] = u[n-1] + Kp*(e[n] - e[n-1])
               + Ki*e[n]
               + Kd*(e[n] - 2*e[n-1] + e[n-2])
u = clamp(u, -2000, +2000)
```

每路电机一份独立状态（基址 0x200000E0，步长 40 字节）：

| 偏移 | 含义 |
|---|---|
| +0x00 | 目标速度（float，mm/s） |
| +0x08 | 累计输出 u（float，PWM 计数） |
| +0x0C | Kp |
| +0x10 | Ki |
| +0x14 | Kd |
| +0x18 | e[n] |
| +0x1C | e[n-2] |
| +0x20 | e[n-1] |

### 5.3 输出级（0x08003060 / 0x08003184 / 0x08002F9C）

```
if (v > 0)      v = v + deadzone;
else if (v < 0) v = v - deadzone;
else            v = 0;
v = clamp(v, -3600, +3600);
// 然后按方向把 |v| 写到一对 CCR 中的一个，另一个写 0
```

注意：PID 输出限幅是 ±2000，最终 PWM 限幅是 ±3600，死区上限 3600。默认死区 1600 相当大（约 44% 占空比），说明这台车的减速电机静摩擦较大。

### 5.4 闭环 / 开环切换

- 全局标志 `0x20000058`：1 = 闭环（PID 生效）
- `spd` 命令 → 置 1，写 4 个目标速度
- `pwm` 命令 → 清 0，直接写 4 个 PWM 值（绕过 PID）
- `mtype == 4` → 定时器中断里直接跳过编码器与 PID，只受 `pwm` 控制

---

## 6. 串口 ASCII 协议

### 6.1 帧格式

```
$<命令名><空格><参数>#          例：$mtype 1#   $spd 300,-300,300,-300#   $mpid 0.8,0.06,0.5#
```

- 命令名**大小写均可**（小写/全大写两套字符串都在固件里）
- 多参数用 **`, `**（逗号+空格）分隔
- 响应不带 `$`/`#`，以 `\r\n` 结尾，**同时从 USART1 和 USART2 输出**
- 无法识别 → `fail,Command not found!\r\n`

### 6.2 命令表

| 命令 | 参数 | 合法范围 | 作用 | 响应 |
|---|---|---|---|---|
| `mtype` | 1 个整数 | 1–4 | 电机/编码器类型；4 = 开环无编码器 | `Set mtype:OK!` / `fail,type error!` |
| `flash_reset` | 无 | – | 参数恢复出厂并写 Flash | `falsh_reset:OK!`（原固件拼写错误） |
| `read_flash` | 无 | – | 以文本逐行输出全部参数 | `read_flash:OK!` + 参数行 |
| `read_vol` | 无 | – | 读电池电压 | `$Battery:%.1fV#` |
| `deadzone` | 1 个整数 | 0–3600 | 死区补偿量 | `Set deadzone:OK!` / `fail,deadzone error!` |
| `mline` | 1 个整数 | u16 | 编码器线数 | `Set mline:OK!` |
| `mphase` | 1 个整数 | u16 | 减速比/相数 | `Set mphase:OK!` |
| `wdiameter` | 1 个浮点 | > 0 | 轮径（mm） | `Set wdiameter:OK!` / `fail,wdiameter error!` |
| `mpid` | 3 个浮点 kp,ki,kd | 校验后写入 | 改 PID；**随后 MCU 软复位** | 实际不会回 `Set MPID:OK!`（见 §9） |
| `upload` | 3 个 `0/1` | – | 选择遥测帧（见 §6.3） | 无 |
| `spd` | 4 个整数 | ±1000 | 闭环目标速度 mm/s | 无 |
| `pwm` | 4 个整数 | ±3600 | 开环直接 PWM | 无 |

`upload` 参数形如 `$upload 1,0,1#`，三个值分别对应 `$MAll`、`$MTEP`、`$MSPD` 的开关。

### 6.3 遥测帧（每当 `upload` 打开时，从两条串口同时输出）

```
$MAll:%d,%d,%d,%d#\r\n      编码器累计位置 ×4
$MTEP:%d,%d,%d,%d#\r\n      每 10 ms 编码器增量 ×4
$MSPD:%.2f,%.2f,%.2f,%.2f#\r\n   实际速度 mm/s ×4（50 ms 节流）
```

`MAll`/`MTEP` 在每个主循环都发（发送函数是阻塞的，速率实际由 115200 波特率限制）；`MSPD` 每 50 ms 最多一帧。

### 6.4 `read_flash` 输出样例（固件内格式串）

```
 Motor_Version:%s\r\n
 Motor_type:%d\r\n
 Dead_Zone:%d\r\n
 Pulse_Line:%d\r\n
 Pulse_Phase:%d\r\n
 wheel_diameter:%.3f\r\n
 P:%.3f\t I:%.3f\t D:%.3f\t \r\n
```

---

## 7. I2C2 从机协议（0x08002A5C / 0x08005218）

- 100 kHz 从机，OAR1 = 0x4C（7 位地址 **0x26**）
- 写事务：3 字节 `[命令][数据高][数据低]`
- 读事务：主机写 1 字节寄存器号，然后读 2 字节（高字节在前）

写命令：

| 命令字节 | 含义 |
|---|---|
| 0 | mtype（仅接受 1–4） |
| 1 | deadzone |
| 2 | mline |
| 3 | mphase |
| 4 | wdiameter（后面跟 ASCII，如 `"67.0"`，用 atof 解析） |
| 5 | spd：8 字节 = 4×int16（大端） |
| 6 | pwm：8 字节 = 4×int16（大端） |
| 7 | 空操作 |

读寄存器：

| 寄存器 | 内容 |
|---|---|
| 0x08 | 电池电压 ×10（u8，例：0x7E = 12.6 V） |
| 0x10–0x13 | 4 路 10 ms 编码器增量 |
| 0x20/0x21 | 位置 0 高 16 / 低 16 |
| 0x22/0x23 | 位置 1 高/低 |
| 0x24/0x25 | 位置 2 高/低 |
| 0x26/0x27 | 位置 3 高/低 |
| 其他 | 返回 0 |

> I2C 读事务的触发细节（是否使用重复起始、主机何时取数）由 0x08002A5C 的事件状态机决定，静态分析可确认命令/响应语义，但完整时序建议用逻辑分析仪抓一次确认。

---

## 8. 参数持久化

- 参数页：Flash `0x08024000`（2 KB 页），魔数 `0xAA55` 存在 `0x08024002`
- 布局（相对 `0x08024010`）：

| 偏移 | 类型 | 参数 |
|---|---|---|
| +0x00 | u16 | mtype |
| +0x04 | u16 | deadzone |
| +0x10 | u16 | mline |
| +0x20 | u16 | mphase |
| +0x40 | float | wdiameter |
| +0x70 | float | Kp |
| +0x76 | float | Ki |
| +0x82 | float | Kd |

- 写入方式：整页读出到 RAM 缓冲 `0x20000378` → 修改半字 → 解锁（0x45670123/0xCDEF89AB）→ 擦除页 → 整页回写。**没有磨损均衡/日志**，每次改参数都擦一次页。
- 上电流程（0x08002454）：读魔数，不合法则写入出厂值并写魔数，然后从 Flash 载入 RAM。

出厂默认值（`0x08002570` 中的立即数/常量）：

| 参数 | 默认值 |
|---|---|
| mtype | 1 |
| deadzone | 1600 |
| mline | 11 |
| mphase | 30 |
| wdiameter | 67.0 mm |
| Kp / Ki / Kd | 0.8 / 0.06 / 0.5 |

---

## 9. 逆向中发现的缺陷 / 注意点

1. **`mpid` 命令会立刻复位 MCU**：写完 Flash 后调用 0x080013E4（打印 `\r\nReset MCU\r\n` → SYSRESETREQ），因此后面的 `Set MPID:OK!` 是**不可达死代码**。上位机不要等这个回包。
2. **命令缺少空格时可造成 RAM 越界写**：参数类命令用 `strlen(buf) - (命令名长度+1)` 计算参数长度并存进 `u8`。例如发 `$deadzone#`（没有空格）会得到 255，然后从 `buf+9` 复制 255 字节到 50 字节缓冲，破坏 RAM。正常格式必须带空格。
3. **`falsh_reset:OK!`** 是固件原文拼写错误（不是分析笔误）。
4. **Flash 参数写无磨损均衡**，且每次 setter 都整页擦写；频繁调参会加速磨损（F103 典型 10k 次擦写）。
5. **I2C 写缓冲无长度检查**，主机异常发送可能越界。
6. **无看门狗配置**（IWDG/WWDG 均未初始化）。
7. **SysTick 未用**，所有节拍依赖 TIM6；TIM6 中断优先级 Preempt=2 / Sub=3。
8. **故障异常全部死循环**，现场无法知道是 HardFault 还是看门狗之外的复位。

---

## 10. 未完全确定的部分

- I2C 读事务的精确时序（建议逻辑分析仪确认）。
- `mtype = 3` 在本固件中与 1 的行为相同（代码只特判了 2 和 4），可能是预留型号。
- TIM1 CH2/CH3 的主输出（PA9/PA10）与 USART1 复用，固件只启用了 N 输出；这与硬件原理图一致，但建议对照板子确认。
- 0x08005048 里用 `SystemCoreClock/1e6` 算出的 `0x20000034/0x20000036` 两个数只在该函数内使用，未见其他消费者，可能是遗留代码。

---

## 11. 分析产物

**本仓库不包含原厂固件**（`FourWay_Motor_STM32.hex`）、由它提取的 `fw.bin`，以及由此生成的
完整反汇编 `listing.txt`、`functions.json`、`fw.bin.json`。这些文件只保留在本地分析环境，
已加入 `.gitignore`。仓库内只保留本报告和分析脚本：

| 文件 | 内容 |
|---|---|
| `REPORT.md` | 本分析报告 |
| `ihex.py` | Intel HEX 解析 |
| `vectors.py` | 向量表/外设地址扫描 |
| `scan.py` | 外设引用与字符串扫描 |
| `xrefs.py` | 字符串/函数指针交叉引用 |
| `disasm.py` | 递归下降反汇编器（生成 listing.txt） |
| `show.py` | 区间反汇编查看器：`python show.py 800330c 80033a0` |

如果你自己持有原厂 HEX，可以按以下方式复现（需要 capstone）：

```bash
python -m pip install capstone
python ihex.py "path/to/FourWay_Motor_STM32.hex" fw.bin
python disasm.py
python show.py 8003a6c 8003b60
```

---

## 12. 如果要改固件

常见需求与改法（都在自己持有的 `fw.bin` 上改完再转回 HEX 即可；本仓库不提供该文件）：

- **改出厂默认参数**：修改 0x08002578 起的立即数与 0x080025EC–0x080025F8 的浮点常量（deadzone/mline/mphase/wdiameter/Kp/Ki/Kd），然后执行一次 `flash_reset` 让新默认值写进参数页。
- **改版本号**：0x08005831 处的 ASCII `1.6.5`（最多 9 字节）。
- **改 PWM 频率**：0x08001252 处 `r0 = 0xE10`（ARR=3599）与 0x08001254 的 PSC。
- **改控制频率**：0x08003AF6 `movw r0,#0x1C1F`（PSC=7199）与 0x08003AFE `movs r0,#0x63`（ARR=99）。
- **改 PID 限幅**：0x0800337E/0x0800338C 的 ±2000.0f 常量。
- **改死区上限/校验范围**：0x08001612 处 `cmp r5,#0xE10`。

> 注意：改 Flash 里的常量后，参数页里的旧值仍然优先；需要 `flash_reset` 或擦除 0x08024000 才会生效。
