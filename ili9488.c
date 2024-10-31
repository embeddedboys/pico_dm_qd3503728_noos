// Copyright (c) 2024 embeddedboys developers

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "pico/time.h"
#define pr_fmt(fmt) "ili9488: " fmt

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pico/stdio.h"
#include "pico/stdio_uart.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "ili9488.h"

/*
 * ili9488 Command Table
 */

#define DRV_NAME "ili9488"

#define pr_debug printf

struct ili9488_priv;

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct ili9488_operations {
    int (*init_display)(struct ili9488_priv *priv);
    int (*reset)(struct ili9488_priv *priv);
    int (*clear)(struct ili9488_priv *priv, u16 clear);
    int (*blank)(struct ili9488_priv *priv, bool on);
    int (*sleep)(struct ili9488_priv *priv, bool on);
    int (*set_var)(struct ili9488_priv *priv);
    int (*set_addr_win)(struct ili9488_priv *priv, int xs, int ys, int xe, int ye);
    int (*set_cursor)(struct ili9488_priv *priv, int x, int y);
};

struct ili9488_display {
    u32                     xres;
    u32                     yres;
    u32                     bpp;
    u32                     rotate;
};

struct ili9488_priv {
    u8                      *buf;

    struct {
        int reset;
        int cs;   /* chip select */
        int rs;   /* register/data select */
        int wr;   /* write signal */
        int rd;   /* read signal */
        int bl;   /* backlight */
        int db[LCD_PIN_DB_COUNT];
    } gpio;

    /* device specific */
    const struct ili9488_operations  *tftops;
    struct ili9488_display           *display;
} g_priv;

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define dm_gpio_set_value(p,v) gpio_put(p, v)
#define mdelay(v) sleep_ms(v)

extern int i80_pio_init(uint8_t db_base, uint8_t db_count, uint8_t pin_wr);
extern int i80_write_buf_rs(void *buf, size_t len, bool rs);

static void fbtft_write_gpio16_wr(struct ili9488_priv *priv, void *buf, size_t len)
{
    u16 data;
    int i;
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
    static u16 prev_data;
#endif

    /* Start writing by pulling down /WR */
    dm_gpio_set_value(priv->gpio.wr, 1);

    while (len) {
        data = *(u16 *)buf;

        /* Start writing by pulling down /WR */
        dm_gpio_set_value(priv->gpio.wr, 0);

        // printf("data : 0x%x\n", data);

        /* Set data */
#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
        if (data == prev_data) {
            dm_gpio_set_value(priv->gpio.wr, 1); /* used as delay */
        } else {
            for (i = 0; i < 16; i++) {
                if ((data & 1) != (prev_data & 1))
                    dm_gpio_set_value(priv->gpio.db[i],
                                      data & 1);
                data >>= 1;
                prev_data >>= 1;
            }
        }
#else
        for (i = 0; i < 16; i++) {
            dm_gpio_set_value(&priv->gpio.db[i], data & 1);
            data >>= 1;
        }
#endif

        /* Pullup /WR */
        dm_gpio_set_value(priv->gpio.wr, 1);

#ifndef DO_NOT_OPTIMIZE_FBTFT_WRITE_GPIO
        prev_data = *(u16 *)buf;
#endif
        buf += 2;
        len -= 2;
    }
}

static void fbtft_write_gpio16_wr_rs(struct ili9488_priv *priv, void *buf, size_t len, bool rs)
{
    dm_gpio_set_value(priv->gpio.rs, rs);
    fbtft_write_gpio16_wr(priv, buf, len);
}

/* rs=0 means writing register, rs=1 means writing data */
#if DISP_OVER_PIO
    #define write_buf_rs(p, b, l, r) i80_write_buf_rs(b, l, r)
#else
    #define write_buf_rs(p, b, l, r) fbtft_write_gpio16_wr_rs(p, b, l, r)
#endif

static int ili9488_write_reg(struct ili9488_priv *priv, int len, ...)
{
    u16 *buf = (u16 *)priv->buf;
    va_list args;
    int i;

    va_start(args, len);
    *buf = (u16)va_arg(args, unsigned int);
    write_buf_rs(priv, buf, sizeof(u16), 0);
    len--;

    /* if there no privams */
    if (len == 0)
        return 0;

    for (i = 0; i < len; i++) {
        *buf = (u16)va_arg(args, unsigned int);
        buf++;
    }

    len *= 2;
    write_buf_rs(priv, priv->buf, len, 1);
    va_end(args);

    return 0;
}
#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__}) / sizeof(int))
#define write_reg(priv, ...) \
    ili9488_write_reg(priv, NUMARGS(__VA_ARGS__), __VA_ARGS__)

static int ili9488_reset(struct ili9488_priv *priv)
{
    dm_gpio_set_value(priv->gpio.reset, 1);
    mdelay(10);
    dm_gpio_set_value(priv->gpio.reset, 0);
    mdelay(10);
    dm_gpio_set_value(priv->gpio.reset, 1);
    mdelay(10);
    return 0;
}


static int ili9488_set_var(struct ili9488_priv *priv)
{
    pr_debug("%s\n", __func__);
    return 0;
}

static int ili9488_init_display(struct ili9488_priv *priv)
{
    pr_debug("%s, writing initial sequence...\n", __func__);
    ili9488_reset(priv);
    // dm_gpio_set_value(&priv->gpio.rd, 1);
    // mdelay(150);

    // Positive Gamma Control
    write_reg(priv, 0xE0, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F);

    // Negative Gamma Control
    write_reg(priv, 0xE1, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F);

    write_reg(priv, 0xC0, 0x17, 0x15);          // Power Control 1
    write_reg(priv, 0xC1, 0x41);                // Power Control 2
    write_reg(priv, 0xC5, 0x00, 0x12, 0x80);    // VCOM Control
    write_reg(priv, 0x36, 0x28);                // Memory Access Control
    write_reg(priv, 0x3A, 0x55);                // Pixel Interface Format RGB565 8080 16-bit
    write_reg(priv, 0xB0, 0x00);                // Interface Mode Control

    // Frame Rate Control
    // write_reg(priv, 0xB1, 0xD0, 0x11);       // 60Hz
    write_reg(priv, 0xB1, 0xD0, 0x14);          // 90Hz

    write_reg(priv, 0xB4, 0x02);                // Display Inversion Control
    write_reg(priv, 0xB6, 0x02, 0x02, 0x3B);    // Display Function Control
    write_reg(priv, 0xB7, 0xC6);                // Entry Mode Set
    write_reg(priv, 0xF7, 0xA9, 0x51, 0x2C, 0x82);  // Adjust Control 3
    write_reg(priv, 0x11);                      // Exit Sleep
    mdelay(60);
    write_reg(priv, 0x29);                      // Display on

    return 0;
}

static int ili9488_set_addr_win(struct ili9488_priv *priv, int xs, int ys, int xe,
                                int ye)
{
    /* set column adddress */
    write_reg(priv, 0x2A, xs >> 8, xs, xe >> 8, xe);

    /* set row address */
    write_reg(priv, 0x2B, ys >> 8, ys, ye >> 8, ye);

    /* write start */
    write_reg(priv, 0x2C);
    return 0;
}

static int ili9488_clear(struct ili9488_priv *priv, u16 clear)
{
    u32 width = priv->display->xres;
    u32 height = priv->display->yres;
    int x, y;

    pr_debug("clearing screen (%d x %d) with color 0x%x\n", width, height, clear);

    priv->tftops->set_addr_win(priv, 0, 0,
                         priv->display->xres - 1,
                         priv->display->yres - 1);

    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
            write_buf_rs(priv, &clear, sizeof(u16), 1);
        }
    }

    return 0;
}

static int ili9488_blank(struct ili9488_priv *priv, bool on)
{
    pr_debug("%s\n", __func__);
    return 0;
}

static int ili9488_sleep(struct ili9488_priv *priv, bool on)
{
    pr_debug("%s\n", __func__);
    return 0;
}

static const struct ili9488_operations default_ili9488_ops = {
    .init_display    = ili9488_init_display,
    .reset           = ili9488_reset,
    .clear           = ili9488_clear,
    .blank           = ili9488_blank,
    .sleep           = ili9488_sleep,
    .set_var         = ili9488_set_var,
    .set_addr_win    = ili9488_set_addr_win,
};

static int ili9488_gpio_init(struct ili9488_priv *priv)
{
    printf("initializing gpios...\n");

#if DISP_OVER_PIO
    gpio_init(priv->gpio.reset);
    // gpio_init(priv->gpio.bl);
    // gpio_init(priv->gpio.cs);
    gpio_init(priv->gpio.rs);
    // gpio_init(priv->gpio.rd);

    gpio_set_dir(priv->gpio.reset, GPIO_OUT);
    // gpio_set_dir(priv->gpio.bl, GPIO_OUT);
    // gpio_set_dir(priv->gpio.cs, GPIO_OUT);
    gpio_set_dir(priv->gpio.rs, GPIO_OUT);
    // gpio_set_dir(priv->gpio.rd, GPIO_OUT);
#else
    int *pp = (int *)&priv->gpio;

    int len = sizeof(priv->gpio)/sizeof(priv->gpio.reset);

    while(len--) {
        gpio_init(*pp);
        gpio_set_dir(*pp, GPIO_OUT);
        pp++;
    }
#endif
    return 0;
}

static int ili9488_hw_init(struct ili9488_priv *priv)
{
    printf("initializing hardware...\n");

#if DISP_OVER_PIO
    i80_pio_init(priv->gpio.db[0], ARRAY_SIZE(priv->gpio.db), priv->gpio.wr);
#endif
    ili9488_gpio_init(priv);

    priv->tftops->init_display(priv);
    /* clear screen to black */
    // priv->tftops->clear(priv, 0x0);

    return 0;
}

static struct ili9488_display default_ili9488_display = {
    .xres   = ILI9488_X_RES,
    .yres   = ILI9488_Y_RES,
    .bpp    = 16,
    .rotate = 0,
};

static void ili9488_video_sync(struct ili9488_priv *priv, int xs, int ys, int xe, int ye, void *vmem16, size_t len)
{
    // pr_debug("video sync: xs=%d, ys=%d, xe=%d, ye=%d, len=%d\n", xs, ys, xe, ye, len);
    priv->tftops->set_addr_win(priv, xs, ys, xe, ye);
    write_buf_rs(priv, vmem16, len * 2, 1);
}

void ili9488_video_flush(int xs, int ys, int xe, int ye, void *vmem16, uint32_t len)
{
    ili9488_video_sync(&g_priv, xs, ys, xe, ye, vmem16, len);
}


/* ########### standlone ######## */
static inline void ili9488_write_cmd(uint16_t cmd)
{
    write_buf_rs(&g_priv, &cmd, sizeof(cmd), 0);
}
#define write_cmd ili9488_write_cmd
static inline void ili9488_write_data(uint16_t data)
{
    write_buf_rs(&g_priv, &data, sizeof(data), 1);
}
#define write_data ili9488_write_data

#include "lvgl/lvgl.h"
void ili9488_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
#if 1
    write_cmd(0x2A);
    write_data(area->x1 >> 8);
    write_data(area->x1);
    write_data(area->x2 >> 8);
    write_data(area->x2);

    /* set row address */
    write_cmd(0x2B);
    write_data(area->y1 >> 8);
    write_data(area->y1);
    write_data(area->y2 >> 8);
    write_data(area->y2);

    /* write start */
    write_cmd(0x2C);
    write_buf_rs(&g_priv, (void *)px_map, lv_area_get_size(area) * 2, 1);
#else
    struct ili9488_priv *priv = &g_priv;
    priv->tftops->set_addr_win(priv, area->x1, area->y1, area->x2, area->y2);
    write_buf_rs(priv, (void *)px_map, lv_area_get_size(area) * 2, 1);
#endif
    lv_disp_flush_ready(disp_drv);
}
/* ########### standlone ######## */

#define BUF_SIZE 64
static int ili9488_probe(struct ili9488_priv *priv)
{
    pr_debug("ili9488 probing ...\n");

    priv->buf = (u8 *)malloc(BUF_SIZE);

    priv->display = &default_ili9488_display;
    priv->tftops = &default_ili9488_ops;

    priv->gpio.bl    = LCD_PIN_BL;
    priv->gpio.reset = LCD_PIN_RST;
    // priv->gpio.rd    = 21;
    priv->gpio.rs    = LCD_PIN_RS;
    priv->gpio.wr    = LCD_PIN_WR;
    priv->gpio.cs    = LCD_PIN_CS;

    /* pin0 - pin15 for I8080 data bus */
    for (int i = LCD_PIN_DB_BASE; i < ARRAY_SIZE(priv->gpio.db); i++)
        priv->gpio.db[i] = i;

    ili9488_hw_init(priv);

    return 0;
}

int ili9488_driver_init(void)
{
    ili9488_probe(&g_priv);
    return 0;
}