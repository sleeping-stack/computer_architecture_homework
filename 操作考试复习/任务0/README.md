# Parallel I/O Experiment (app_parallel_IO)

AC850 并行接口实验 — 读取 8 位拨码开关并实时映射到 8 个 LED，5 个按键选择数码管显示的字母。

## 硬件

| AXI GPIO 实例 | 基地址 | Channel 1 | Channel 2 | 功能 |
|--------------|--------|-----------|-----------|------|
| `AXI_GPIO_0` | `XPAR_AXI_GPIO_0_BASEADDR` | 8-bit 开关（输入） | 8-bit LED（输出） | SW → LED |
| `AXI_GPIO_1` | `XPAR_AXI_GPIO_1_BASEADDR` | 8-bit 段码（输出） | 8-bit 位选（输出） | 数码管 |
| `AXI_GPIO_2` | `XPAR_AXI_GPIO_2_BASEADDR` | 5-bit 按键（输入，mask `0x1F`） | — | 按键 |

- **中断控制器**：AXI Interrupt Controller (`XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR`)

### 数码管段码（共阳极）

| 字母 | 段码 |
|------|------|
| C | `0xC6` |
| U | `0xC1` |
| L | `0xC7` |
| D | `0xA1` |
| R | `0xAF` |

## 功能

- 8 位拨码开关状态**实时**同步到 8 个 LED
- 5 个按键按下时，8 位数码管全部显示对应字母（C / U / L / R / D）
- 动态扫描刷新，无闪烁

## 三种实现版本

通过 `UserConfig.cmake` 中的 `IMPL_VERSION` 变量选择编译版本：

| 版本 | 文件 | 技术 | GPIO 访问 | 中断 |
|------|------|------|-----------|------|
| V1 | `impl_v1.c` | 轮询 | `Xil_In8/Out8` 直接寄存器 | 无 |
| V2 | `impl_v2.c` | 普通中断 | `Xil_In32/Out32` 直接寄存器 | `microblaze_register_handler` |
| V3 | `impl_v3.c` | 快速中断 | `Xil_In32/Out32` 直接寄存器 | IVAR 写入 (`fast_interrupt`) |

### V1 — 轮询版本

- 主循环内直接轮询按键状态
- 使用 `Xil_In8/Out8` 等底层寄存器宏（`xgpio_l.h`）
- 每个数码管位停留 2ms（`usleep(2000)`）

### V2 — 普通中断版本

- 按键通过 `microblaze_register_handler()` 注册普通 ISR（`ButtonISR`，带 `CallbackRef` 参数）
- `DelayAndRefresh()` 忙等期间持续刷新开关→LED
- 数码管位间插入消隐（写入 `0x00` 到阳极）防止鬼影
- ISR 中清除两级中断：GPIO ISR + INTC IAR

### V3 — 快速中断版本

- 按键通过 IVAR 寄存器写入 ISR 地址，`__attribute__((fast_interrupt))` 声明
- ISR 无参数，硬件自动应答 INTC（无需手动清除 IAR）
- 初始化额外配置 IMR（中断模式）+ IVAR（向量地址）
- 其余与 V2 相同

## 构建

在 Vitis IDE 中打开项目并构建。通过 `UserConfig.cmake` 选择版本：

```cmake
# 1 = 轮询, 2 = 普通中断, 3 = 快速中断
set(IMPL_VERSION 3)

set(USER_COMPILE_SOURCES "impl_v${IMPL_VERSION}.c")
```

## 文件

| 文件 | 说明 |
|------|------|
| `impl_v1.c` | 轮询版本 |
| `impl_v2.c` | 普通中断版本 |
| `impl_v3.c` | 快速中断版本 |
| `example_1.c` | 教学视频配套测试（串口交互式分块测试各外设） |
| `UserConfig.cmake` | CMake 配置（版本选择、编译选项） |
