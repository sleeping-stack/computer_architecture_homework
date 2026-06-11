/**
 * kb_display.c
 * 按键 + 数码管 + LED + 拨码开关 外设模块
 * 平台: AC850开发板 (MicroBlaze, XC7Z020)
 *
 * 作为 app_final_project 的子模块，负责:
 *   GPIO0 Ch1 = SW拨码开关(8) input   — 波形选择 (中断线4)
 *   GPIO0 Ch2 = LED(8) output          — 通道指示
 *   GPIO1 Ch1 = seg段选(8) output   GPIO1 Ch2 = an位选(8) output
 *   GPIO3 Ch1 = row行(4)  input     GPIO3 Ch2 = col列(4) output
 *
 * 注意: DAC/SPI/Timer/DDS/UART 由 app_final_project 其他模块负责
 */

#include "kb_display.h"
#include "dds.h"       /* DDS_MAX_AMPLITUDE, dds_set_params */
#include "uart.h"      /* g_dds_params, g_last_ch, CTRL_CH_*, DDS_Params_t */
#include "xil_io.h"
#include "xgpio_l.h"
#include "xintc_l.h"
#include "xparameters.h"

/* ================================================================
 * 硬件地址
 * ================================================================ */
#define DISP_BASEADDR    XPAR_AXI_GPIO_1_BASEADDR          /* 0x40010000 */
#define KEY_BASEADDR     XPAR_AXI_GPIO_3_BASEADDR          /* 0x40030000 */
#define SW_BASEADDR      XPAR_AXI_GPIO_0_BASEADDR          /* 0x40000000 */
#define BTN_BASEADDR     XPAR_AXI_GPIO_2_BASEADDR          /* 0x40020000 */
#define INTC_BASEADDR    XPAR_MICROBLAZE_0_AXI_INTC_BASEADDR /* 0x41200000 */

/* 中断掩码 */
#define GPIO_KEY_INTR_MASK  (1 << XPAR_FABRIC_AXI_GPIO_3_INTR)  /* bit2 */
#define GPIO_SW_INTR_MASK   (1 << XPAR_FABRIC_AXI_GPIO_0_INTR)  /* bit4 */
#define GPIO_BTN_INTR_MASK  (1 << XPAR_FABRIC_AXI_GPIO_2_INTR)  /* bit3 */

/* LED 模式指示位 */
#define LED_MODE_SYNC    0x10   /* bit4: 同步模式LED亮 */

/* ================================================================
 * 系统参数
 * ================================================================ */
// F_SAMPLE 在 dds.h 中定义 (100000UL), 此处不再重复定义
#define DISP_DIVIDER     (F_SAMPLE / 800)    /* 显示扫描分频: 100000/800=125, ~800Hz/8位≈100Hz刷新 */
#define DISPLAY_DIGITS   8

/* 编辑缓冲长度 */
#define VAR_MAX          4

/* ================================================================
 * 按键编码
 * ================================================================ */
#define KEY_0         0
#define KEY_1         1
#define KEY_2         2
#define KEY_3         3
#define KEY_4         4
#define KEY_5         5
#define KEY_6         6
#define KEY_7         7
#define KEY_8         8
#define KEY_9         9
#define KEY_CONFIRM   10
#define KEY_BACKSPACE 11
#define KEY_RESET     12
#define KEY_SWITCH    13
#define KEY_CH1       14   /* 选择通道1 (DAC A) */
#define KEY_CH2       15   /* 选择通道2 (DAC B) */
#define KEY_NONE      0xFF

/* ================================================================
 * 段码表 - 共阳极 (0=亮, 1=灭)
 * 0~F 对应: 0xC0,0xF9,0xA4,0xB0,0x99,0x92,0x82,0xF8,
 *           0x80,0x90,0x88,0x83,0xC6,0xA1,0x86,0x8E
 * ================================================================ */
static const unsigned char SegCode[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E
};

/* ================================================================
 * 位选表 - 共阳极 (MSB=左1, LSB=右8)
 * ================================================================ */
static const unsigned char DigitSel[DISPLAY_DIGITS] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

/* ================================================================
 * 键盘映射表 (4x4)
 *
 *   Col0  Col1  Col2      Col3
 *   [0]   [1]   [2]       [3]
 *   [4]   [5]   [6]       [7]
 *   [8]   [9]   [Back]    [Confirm]
 *   [Rst] [Sw]  [CH1]     [CH2]
 * ================================================================ */
static const unsigned char key_map[4][4] = {
    { KEY_0,    KEY_1,       KEY_2,          KEY_3        },
    { KEY_4,    KEY_5,       KEY_6,          KEY_7        },
    { KEY_8,    KEY_9,       KEY_BACKSPACE,  KEY_CONFIRM  },
    { KEY_RESET, KEY_SWITCH, KEY_CH1,        KEY_CH2      }
};

/* ================================================================
 * 全局变量
 * ================================================================ */

/* 显示缓冲: -1=该位不显示, 0~15=段码索引 */
static int displayNum[DISPLAY_DIGITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* 频率和幅度编辑缓冲 (全局, 切换通道时重新加载) */
static unsigned char freq_buffer[VAR_MAX];
static int           freq_count = 0;
static unsigned char amp_buffer[VAR_MAX];
static int           amp_count = 0;

/* 当前编辑: 0=频率(左边4位), 1=幅度(右边4位) */
static volatile int  current_edit = 0;

/* 键盘消抖 */
static volatile int           key_stable    = 0;
static volatile unsigned char current_key   = KEY_NONE;
static volatile int           key_ready     = 0;

/* 当前活动通道: 0=通道A, 1=通道B */
static volatile uint8_t g_active_channel = 0;
static volatile int edit_locked = 1;           /* 编辑锁定: 1=锁定, 0=可编辑 */

/* 模式按键消抖 */
static volatile int mode_btn_stable = 0;
/* ================================================================
 * 函数声明
 * ================================================================ */
static void Seg_Init(void);
static void Seg_Display_Buffer(void);
static void value_to_buffer(uint32_t value, unsigned char *buf, int *count);
static uint32_t buffer_to_value(unsigned char *buf, int count);
static void channel_load_to_edit(uint8_t ch);
static void channel_save_from_edit(uint8_t ch);
static void Key_Init(void);
static void SW_Init(void);
static void process_key(unsigned char key);

/* ================================================================
 * update_led - 统一 LED 写入, 自动合并模式指示位
 * ch_led: 通道指示位 (0x03=CH1, 0x0C=CH2)
 * ================================================================ */
static void update_led(uint8_t ch_led)
{
    uint8_t led = ch_led;
    if (g_dds_params[0].ctrl & CTRL_MODE_SYNC)
        led |= LED_MODE_SYNC;
    Xil_Out8(SW_BASEADDR + XGPIO_DATA2_OFFSET, led);
}
/* ================================================================
 * Seg_Init - 数码管 GPIO1 初始化
 * ================================================================ */
static void Seg_Init(void)
{
    Xil_Out8(DISP_BASEADDR + XGPIO_TRI_OFFSET,  0x00);
    Xil_Out8(DISP_BASEADDR + XGPIO_TRI2_OFFSET, 0x00);
}

/* ================================================================
 * kb_display_tick - 数码管动态扫描 (定时器ISR中调用, 100kHz)
 * 内部分频至 ~800Hz 扫描, 消影(0x00) -> 位选 -> 段选
 * ================================================================ */
void kb_display_tick(void)
{
    static int div_counter = 0;
    static int Seg_Sel = 0;

    /* 软件分频: 100kHz / DISP_DIVIDER ≈ 800Hz 扫描率 */
    if (++div_counter < DISP_DIVIDER) return;
    div_counter = 0;

    /* 消影: 关闭全部位选 */
    Xil_Out8(DISP_BASEADDR + XGPIO_DATA2_OFFSET, 0x00);

    /* 位选 + 段选 (仅有效位) */
    if (displayNum[Seg_Sel] >= 0 && displayNum[Seg_Sel] <= 15) {
        Xil_Out8(DISP_BASEADDR + XGPIO_DATA2_OFFSET, DigitSel[Seg_Sel]);
        Xil_Out8(DISP_BASEADDR + XGPIO_DATA_OFFSET,
                 SegCode[displayNum[Seg_Sel]]);
    }

    /* 移到下一位 */
    if (++Seg_Sel >= DISPLAY_DIGITS) Seg_Sel = 0;
}

/* ================================================================
 * value_to_buffer - 将数值转换为段码数字缓冲
 * ================================================================ */
static void value_to_buffer(uint32_t value, unsigned char *buf, int *count)
{
    unsigned char digits[VAR_MAX];
    int n = 0;

    if (value == 0) {
        buf[0] = 0;
        *count = 1;
        return;
    }

    while (value > 0 && n < VAR_MAX) {
        digits[n++] = value % 10;
        value /= 10;
    }

    for (int i = 0; i < n; i++) {
        buf[i] = digits[n - 1 - i];
    }
    *count = n;
}

/* ================================================================
 * buffer_to_value - 将段码数字缓冲转换为数值
 * ================================================================ */
static uint32_t buffer_to_value(unsigned char *buf, int count)
{
    uint32_t val = 0;
    for (int i = 0; i < count; i++) {
        val = val * 10 + buf[i];
    }
    return val;
}

/* ================================================================
 * channel_load_to_edit - 从 g_dds_params[ch] 加载参数到编辑缓冲
 * ================================================================ */
static void channel_load_to_edit(uint8_t ch)
{
    value_to_buffer(g_dds_params[ch].frequency, freq_buffer, &freq_count);
    value_to_buffer(g_dds_params[ch].amplitude,  amp_buffer,  &amp_count);
    Seg_Display_Buffer();
}

/* ================================================================
 * channel_save_from_edit - 将编辑缓冲保存到 g_dds_params[ch],
 * 并调用 app_final_project 的 dds_set_params() 更新 DDS 状态
 * ================================================================ */
static void channel_save_from_edit(uint8_t ch)
{
    uint32_t freq = 0;
    uint32_t amp  = 0;

    if (freq_count > 0) {
        freq = buffer_to_value(freq_buffer, freq_count);
    }
    if (amp_count > 0) {
        amp = buffer_to_value(amp_buffer, amp_count);
        if (amp > DDS_MAX_AMPLITUDE) amp = DDS_MAX_AMPLITUDE;
    }

    if (g_dds_params[0].ctrl & CTRL_MODE_SYNC) {
        /* 同步模式: 写入两个通道 + 相位对齐 (与 UART 同步模式处理一致) */
        g_dds_params[0].frequency = freq;
        g_dds_params[0].amplitude = amp;
        g_dds_params[0].ctrl |= CTRL_MODE_SYNC;
        g_dds_params[1].frequency = freq;
        g_dds_params[1].amplitude = amp;
        g_dds_params[1].ctrl |= CTRL_MODE_SYNC;
        g_dds_params[1].wave_type = g_dds_params[0].wave_type;
        dds_set_params(0, freq, amp, g_dds_params[0].wave_type);
        dds_set_params(1, freq, amp, g_dds_params[1].wave_type);
        dds_align_phase();
    } else {
        /* 独立模式: 仅更新当前通道 (与 UART 独立模式处理一致) */
        g_dds_params[ch].frequency = freq;
        g_dds_params[ch].amplitude = amp;
        g_dds_params[ch].ctrl      = (ch == 0) ? CTRL_CH_CH1 : CTRL_CH_CH2;
        dds_set_params(ch, freq, amp, g_dds_params[ch].wave_type);
    }
}

/* ================================================================
 * Seg_Display_Buffer - 将频率/幅度缓冲映射到 displayNum
 * 左边4位 = 频率 (右对齐)
 * 右边4位 = 幅度 (右对齐)
 * displayNum[i] = -1 -> 该位消隐
 * ================================================================ */
static void Seg_Display_Buffer(void)
{
    int i, start;

    /* 左边4位: 频率 */
    start = 4 - freq_count;
    for (i = 0; i < 4; i++) {
        if (i >= start)
            displayNum[i] = freq_buffer[i - start];
        else
            displayNum[i] = -1;
    }

    /* 右边4位: 幅度 */
    start = 4 - amp_count;
    for (i = 0; i < 4; i++) {
        if (i >= start)
            displayNum[4 + i] = amp_buffer[i - start];
        else
            displayNum[4 + i] = -1;
    }
}

/* ================================================================
 * Key_Init - 矩阵键盘 GPIO3 初始化
 * Ch1=行输入(TRI=1)  Ch2=列输出(TRI2=0)
 * ================================================================ */
static void Key_Init(void)
{
    Xil_Out32(KEY_BASEADDR + XGPIO_TRI_OFFSET,  0x0f);
    Xil_Out32(KEY_BASEADDR + XGPIO_TRI2_OFFSET, 0x00);

    /* 所有列驱动为低 */
    Xil_Out32(KEY_BASEADDR + XGPIO_DATA2_OFFSET, 0x00);

    /* 使能行线通道中断 */
    Xil_Out32(KEY_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(KEY_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(KEY_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);
}

/* ================================================================
 * SW_Init - 拨码开关 GPIO0 初始化 + 中断使能
 * Ch1=输入(TRI=0xFF)  用于波形选择
 * Ch2=输出(TRI2=0x00) 用于LED指示
 * ================================================================ */
static void SW_Init(void)
{
    /* Ch1: 开关 -> 输入 (TRI=1) */
    Xil_Out8(SW_BASEADDR + XGPIO_TRI_OFFSET, 0xFF);

    /* Ch2: LED -> 输出 (TRI2=0) */
    Xil_Out8(SW_BASEADDR + XGPIO_TRI2_OFFSET, 0x00);

    /* 使能 Ch1 通道中断 (边沿检测) */
    Xil_Out32(SW_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(SW_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(SW_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);
}

/* ================================================================
 * Btn_Init - 模式按键 GPIO2 初始化
 * Ch1=输入(TRI=0xFF)  bit0 为模式切换按键
 * ================================================================ */
static void Btn_Init(void)
{
    /* Ch1: 按键 -> 输入 (TRI=1) */
    Xil_Out8(BTN_BASEADDR + XGPIO_TRI_OFFSET, 0xFF);

    /* 使能 Ch1 通道中断 (边沿检测) */
    Xil_Out32(BTN_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(BTN_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(BTN_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);
}
/* ================================================================
 * key_scan_isr - 矩阵键盘扫描 (在 GPIO3 ISR 中调用)
 * 逐列驱动为低, 读行值, 交叉点即为按键
 * ================================================================ */
void key_scan_isr(void)
{
    int row, col, row_val;
    unsigned char key;

    row_val = Xil_In32(KEY_BASEADDR + XGPIO_DATA_OFFSET) & 0x0f;

    if (row_val == 0x0f) {
        key_stable = 0;
        return;
    }

    if (key_stable)
        return;

    key = KEY_NONE;

    for (col = 0; col < 4; col++) {
        Xil_Out32(KEY_BASEADDR + XGPIO_DATA2_OFFSET,
                  (~(1 << col)) & 0x0f);

        {
            volatile int d;
            for (d = 0; d < 50; d++) { /* spin */ }
        }

        row_val = Xil_In32(KEY_BASEADDR + XGPIO_DATA_OFFSET) & 0x0f;

        if (row_val != 0x0f) {
            for (row = 0; row < 4; row++) {
                if ((row_val & (1 << row)) == 0) {
                    key = key_map[row][col];
                    break;
                }
            }
            break;
        }
    }

    Xil_Out32(KEY_BASEADDR + XGPIO_DATA2_OFFSET, 0x00);

    if (key != KEY_NONE) {
        current_key = key;
        key_ready   = 1;
        key_stable  = 1;
    }
}

/* ================================================================
 * sw_isr - 拨码开关中断处理 (在 GPIO0 ISR 中调用)
 * 读取开关值，更新 g_dds_params 并调用 dds_set_params() 推入 DDS 引擎
 * ================================================================ */
void sw_isr(void)
{
    uint8_t sw = Xil_In8(SW_BASEADDR + XGPIO_DATA_OFFSET);

    /* 写入 app_final_project 的参数结构 (与 UART 路径一致) */
    if (g_dds_params[0].ctrl & CTRL_MODE_SYNC) {
        /* 同步模式: 两通道波形均跟随 CH1 开关 (bits[1:0]) */
        g_dds_params[0].wave_type = sw & 0x03;
        g_dds_params[1].wave_type = sw & 0x03;
        dds_set_params(0, g_dds_params[0].frequency, g_dds_params[0].amplitude,
                       g_dds_params[0].wave_type);
        dds_set_params(1, g_dds_params[1].frequency, g_dds_params[1].amplitude,
                       g_dds_params[1].wave_type);
    } else {
        g_dds_params[0].wave_type = sw & 0x03;
        g_dds_params[1].wave_type = (sw >> 2) & 0x03;
        dds_set_params(0, g_dds_params[0].frequency, g_dds_params[0].amplitude,
                       g_dds_params[0].wave_type);
        dds_set_params(1, g_dds_params[1].frequency, g_dds_params[1].amplitude,
                       g_dds_params[1].wave_type);
    }

    Xil_Out32(SW_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}

/* ================================================================
 * mode_btn_isr - 模式按键中断处理 (在 GPIO2 ISR 中调用)
 * 每按一次翻转 CTRL_MODE_SYNC, 同时更新 LED 模式指示
 * 默认上电为独立模式 (dds_init 设置 ctrl=0)
 * ================================================================ */
void mode_btn_isr(void)
{
    uint8_t btn = Xil_In8(BTN_BASEADDR + XGPIO_DATA_OFFSET);

    /* 按键释放检测 (bit0 从 0 回到 1) */
    if ((btn & 0x01) && mode_btn_stable) {
        mode_btn_stable = 0;
    }
    /* 按键按下检测 (bit0 = 0, 下降沿) */
    if (!(btn & 0x01) && !mode_btn_stable) {
        mode_btn_stable = 1;
        /* 翻转 CTRL_MODE_SYNC 位 */
        uint8_t new_mode = (g_dds_params[0].ctrl & CTRL_MODE_SYNC)
                           ? 0 : CTRL_MODE_SYNC;
        g_dds_params[0].ctrl = (g_dds_params[0].ctrl & ~CTRL_MODE_SYNC) | new_mode;
        g_dds_params[1].ctrl = (g_dds_params[1].ctrl & ~CTRL_MODE_SYNC) | new_mode;
        if (new_mode) {
            /* 进入同步模式: wave_type 以 CH1 为准 */
            g_dds_params[1].wave_type = g_dds_params[0].wave_type;
        }
        /* 更新 LED 模式指示 */
        uint8_t ch_led = (g_active_channel == 0) ? 0x03 : 0x0C;
        update_led(ch_led);
    }

    Xil_Out32(BTN_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
}
/* ================================================================
 * process_key - 主循环调用 (通过 kb_display_poll)
 * CH1/CH2: 切换编辑通道, 加载该通道参数
 * SWITCH: 切换频率<->幅度编辑
 * CONFIRM: 将编辑缓冲保存到当前通道
 * RESET:   清零当前编辑缓冲
 * 数字键/退格: 编辑当前变量
 * ================================================================ */
static void process_key(unsigned char key)
{
    /* 通道选择 (不受编辑状态影响, 随时可切换) */
    if (key == KEY_CH1) {
        g_active_channel = 0;
        g_last_ch = 0;  /* 同步 app_final_project 的通道变量 */
        update_led(0x03);  /* LED: 通道A (自动合并模式指示) */
        edit_locked = 1;
        channel_load_to_edit(0);
        return;
    }
    if (key == KEY_CH2) {
        g_active_channel = 1;
        g_last_ch = 1;
        update_led(0x0C);  /* LED: 通道B (自动合并模式指示) */
        edit_locked = 1;
        channel_load_to_edit(1);
        return;
    }

    /* 切换频率/幅度编辑 (不受锁定影响) */
    if (key == KEY_SWITCH) {
        current_edit = 1 - current_edit;
        return;
    }

    /* 根据当前编辑变量选择操作缓冲 */
    unsigned char *buf;
    int *count;
    if (current_edit == 0) {
        buf = freq_buffer;
        count = &freq_count;
    } else {
        buf = amp_buffer;
        count = &amp_count;
    }

    /* 锁定状态下仅复位键有效 */
    if (edit_locked && key != KEY_RESET)
        return;

    switch (key) {
    case KEY_0: case KEY_1: case KEY_2: case KEY_3:
    case KEY_4: case KEY_5: case KEY_6: case KEY_7:
    case KEY_8: case KEY_9:
        if (*count < VAR_MAX) {
            buf[*count] = key;
            (*count)++;
            Seg_Display_Buffer();
        }
        break;
    case KEY_BACKSPACE:
        if (*count > 0) {
            (*count)--;
            Seg_Display_Buffer();
        }
        break;
    case KEY_RESET:
        *count = 0;
        edit_locked = 0;
        Seg_Display_Buffer();
        break;
    case KEY_CONFIRM:
        if (*count > 0) {
            channel_save_from_edit(g_active_channel);
            edit_locked = 1;
            /* 确认后重新加载显示 (显示确认后的完整值) */
            channel_load_to_edit(g_active_channel);
        }
        break;
    default:
        break;
    }
}

/* ================================================================
 * 公开接口
 * ================================================================ */

/* kb_display_init: 初始化所有外设 GPIO + INTC 中断注册 + 默认显示 */
void kb_display_init(void)
{
    Seg_Init();
    Key_Init();
    SW_Init();
    Btn_Init();

    /* 在 INTC 中注册键盘、开关和模式按键中断 (追加到已有的 IER 值) */
    Xil_Out32(INTC_BASEADDR + XIN_IER_OFFSET,
              Xil_In32(INTC_BASEADDR + XIN_IER_OFFSET)
              | GPIO_KEY_INTR_MASK | GPIO_SW_INTR_MASK | GPIO_BTN_INTR_MASK);

    /* 初始 LED 指示: 通道 A, 独立模式 (默认) */
    update_led(0x03);

    /* 从 g_dds_params 加载默认参数到显示 (dds_init 已设置默认值) */
    g_active_channel = 0;
    channel_load_to_edit(0);
}

/* kb_display_poll: 主循环中调用, 检查并处理按键 */
void kb_display_poll(void)
{
    if (key_ready) {
        process_key(current_key);
        key_ready = 0;
    }
}

/* display_sync_from_uart: UART 改变参数后同步 LED 和数码管 */
void display_sync_from_uart(uint8_t ch)
{
    /* 更新 LED (通道指示 + 模式指示自动合并) */
    update_led((ch == 0) ? 0x03 : 0x0C);

    /* 同步通道变量 */
    g_active_channel = ch;

    /* 从 g_dds_params 加载参数到显示缓冲 */
    channel_load_to_edit(ch);
}

