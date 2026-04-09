// Copyright (c) 2026 embeddedboys developers
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "pico/time.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/stdio_usb.h"
#include "pico/stdio_uart.h"

#include "hardware/pll.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"

#include "ili9488.h"
#include "ft6236.h"
#include "backlight.h"

#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lvgl/examples/lv_examples.h"

/*
 * Whether to use core1 to run lvgl tasks.
 *
 * NOTE: Avoid race conditions between two
 * cores accessing XIP as much as possible.
 */
#define LVGL_USE_CORE1 0

#ifndef MY_DISP_BUF_SIZE
#warning '"MY_DISP_BUF_SIZE" is not defined, defaulting to (HOR_RES * VER_RES / 2)'
#define MY_DISP_BUF_SIZE (MY_DISP_HOR_RES * MY_DISP_VER_RES / 2)
#endif

static void __attribute__((section(".time_critical.lvgl")))
my_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
	ili9488_video_flush(area->x1, area->y1, area->x2, area->y2,
			    (void *)color_p,
			    lv_area_get_size(area) * sizeof(lv_color_t));

	lv_disp_flush_ready(disp_drv);
}

/*Will be called by the library to read the touchpad*/
static void __attribute__((section(".time_critical.lvgl")))
my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
	static lv_coord_t last_x = 0;
	static lv_coord_t last_y = 0;

	/*Save the pressed coordinates and the state*/
	if (ft6236_is_pressed()) {
		last_x = ft6236_read_x();
		last_y = ft6236_read_y();
		// printf("touchpad is pressed, x: %d, y: %d\n", last_x, last_y);
		data->state = LV_INDEV_STATE_PR;
	} else {
		data->state = LV_INDEV_STATE_REL;
	}

	/*Set the last pressed coordinates*/
	data->point.x = last_x;
	data->point.y = last_y;
}

static void my_hardware_init(void)
{
	/* NOTE: DO NOT MODIFY THIS BLOCK */
#define CPU_SPEED_MHZ (DEFAULT_SYS_CLK_KHZ / 1000)
	if (CPU_SPEED_MHZ > 266 && CPU_SPEED_MHZ <= 360)
		vreg_set_voltage(VREG_VOLTAGE_1_20);
	else if (CPU_SPEED_MHZ > 360 && CPU_SPEED_MHZ <= 396)
		vreg_set_voltage(VREG_VOLTAGE_1_25);
	else if (CPU_SPEED_MHZ > 396)
		vreg_set_voltage(VREG_VOLTAGE_MAX);
	else
		vreg_set_voltage(VREG_VOLTAGE_DEFAULT);

	set_sys_clock_khz(CPU_SPEED_MHZ * 1000, true);
	clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
			CPU_SPEED_MHZ * MHZ, CPU_SPEED_MHZ * MHZ);
	stdio_uart_init_full(uart0, 115200, 16, 17);
	stdio_usb_init();

	ili9488_driver_init();
	ft6236_driver_init();

	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
}

#if LVGL_USE_CORE1
static void core1_entry(void)
{
	for (;;) {
		lv_timer_handler_run_in_period(1);
	}
}
#endif

int main(void)
{
	printf("\n\n\nPICO DM QD3503728 LVGL(release/v8.4.0) Porting\n");

	my_hardware_init();

	/*Initialize LVGL*/
	lv_init();

	static lv_disp_draw_buf_t draw_buf_dsc_1;
	static lv_color_t buf_1[MY_DISP_BUF_SIZE];

	/*Initialize the display buffer*/
	lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_BUF_SIZE);

	/*Descriptor of a display driver*/
	static lv_disp_drv_t disp_drv;

	/*Basic initialization*/
	lv_disp_drv_init(&disp_drv);

	/*Set the resolution of the display*/
	disp_drv.hor_res = LCD_HOR_RES;
	disp_drv.ver_res = LCD_VER_RES;

	/*Used to copy the buffer's content to the display*/
	disp_drv.flush_cb = my_flush_cb;

	/*Set a display buffer*/
	disp_drv.draw_buf = &draw_buf_dsc_1;

	/*Finally register the driver*/
	lv_disp_drv_register(&disp_drv);

	/*Create an input device for touch handling*/
	static lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = my_touchpad_read;
	lv_indev_drv_register(&indev_drv);

	printf("Starting demo\n");
	lv_demo_widgets();
	// lv_demo_keypad_encoder();
	// lv_demo_stress();
	// lv_demo_music();

	/* measure weighted fps and opa speed */
	// Before : Avg.146 256 114 186
	// After  : Avg.181 282 150 222
	// lv_demo_benchmark();

	/* This is a factory test app */
	// extern int factory_test(void);
	// factory_test();

	sleep_ms(10);
	backlight_driver_init();
	backlight_set_level(100);
	printf("backlight set to 100%%\n");

#if LVGL_USE_CORE1
	multicore_launch_core1(core1_entry);
#endif

	printf("going to loop, %lld\n", time_us_64() / 1000);
	for (;;) {
#if LVGL_USE_CORE1
		tight_loop_contents();
		sleep_ms(200);
#else
		lv_timer_handler_run_in_period(1);
#endif
	}

	return 0;
}
