# Pico_DM_QD3503728

这是一个基于 Raspberry Pi Pico 和 DM-QD3503728 显示面板的 LVGL GUI 库移植项目。项目包含了完整的显示驱动和 LVGL 集成，适用于 3.5 英寸 480x320 分辨率的 TFT 显示屏。

#### Raspberrypi Pico Display Module with LVGL, Micropython support start at 69.8￥(9.9$)

![dm-qd3503728-0](assets/dm-qd3503728-0.jpg)
![dm-qd3503728-1](assets/dm-qd3503728-1.jpg)

## 特性

- ✅ LVGL 8.4.0 支持
- ✅ PIO + DMA 加速的帧缓冲刷新
- ✅ PWM 背光控制

## 硬件规格

| 部件     | 型号/规格                   |
| -------- | --------------------------- |
| 核心板   | Raspberry Pi Pico (RP2040)  |
| 显示屏   | 3.5英寸 TFT LCD ILI9488     |
| 分辨率   | 480 × 320 像素              |
| 接口     | 16位 I8080 并行接口 @ 50MHz |
| 触摸屏   | 电容式 FT6236               |
| 颜色深度 | 16位 RGB565 (65K 颜色)      |

### Technical specifications

| Part        | Model                       |
| ----------- | --------------------------- |
| Core Board  | Rasberrypi Pico             |
| Display     | 3.5' 480x320 ILI9488 no IPS |
|             | 16-bit 8080 50MHz           |
| TouchScreen | 3.5' FT6236 capacity touch  |

## 引脚映射

### 显示面板连接

| Pico GPIO  | 显示面板信号 | 功能说明                     |
| ---------- | ------------ | ---------------------------- |
| GP0 ~ GP15 | DB0-DB15     | 16位数据总线                 |
| GP18       | CS           | 片选信号（低有效）           |
| GP19       | WR           | 写使能信号（低有效）         |
| GP20       | RS           | 寄存器选择（0:命令, 1:数据） |
| GP22       | RESET        | 复位信号（低有效）           |
| GP28       | BL           | 背光控制（高电平亮）         |

| Pico 左侧引脚 | Pico 右侧引脚           |
| ------------- | ----------------------- |
| GP0/DB0       | VBUS (+5V)              |
| GP1/DB1       | VSYS (主电源)           |
| GND           | GND                     |
| GP2/DB2       | 3V3_EN (3.3V使能)       |
| GP3/DB3       | GP29                    |
| GP4/DB4       | GND                     |
| GP5/DB5       | GP28/BL (背光)          |
| GP6/DB6       | GP27                    |
| GP7/DB7       | GP26                    |
| GND           | RUN (复位)              |
| GP8/DB8       | GP22/RESET (显示屏复位) |
| GP9/DB9       | GND                     |
| GP10/DB10     | GP21                    |
| GP11/DB11     | GP20/RS (寄存器选择)    |
| GP12/DB12     | GP19/WR (写使能)        |
| GP13/DB13     | GP18/CS (片选)          |
| GND           | GND                     |
| GP14/DB14     | GP17                    |
| GP15/DB15     | GP16                    |

## 故障排除

### 常见问题

1. **显示屏不亮**
      - 检查背光引脚（GP28）连接
      - 确认电源连接正确
      - 检查复位信号（GP22）

2. **图像显示异常**
      - 确认数据线连接正确（GP0-GP15）
      - 检查 WR、CS、RS 时序信号
      - 验证 LVGL 颜色格式配置

3. **编译错误**
      - 确保安装了正确的工具链
      - 更新子模块：`git submodule update --init --recursive`
      - 清除构建目录重新构建

## 许可证

本项目基于 MIT 许可证发布 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 贡献

欢迎提交 Issue 和 Pull Request！

## 相关项目

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [LVGL - Light and Versatile Graphics Library](https://github.com/lvgl/lvgl)
- [Pico Extras](https://github.com/raspberrypi/pico-extras)
