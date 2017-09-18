/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Hammer board configuration */

#include "common.h"
#include "ec_version.h"
#include "ec_ec_comm_slave.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "printf.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "queue.h"
#include "queue_policies.h"
#include "registers.h"
#include "rollback.h"
#include "system.h"
#include "task.h"
#include "touchpad.h"
#include "timer.h"
#include "update_fw.h"
#include "usart-stm32f0.h"
#include "usart_tx_dma.h"
#include "usart_rx_dma.h"
#include "usb_descriptor.h"
#include "usb_i2c.h"
#include "util.h"

#include "gpio_list.h"

#ifdef SECTION_IS_RW
#define CROS_EC_SECTION "RW"
#else
#define CROS_EC_SECTION "RO"
#endif

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */
const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("Hammer"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      =
			USB_STRING_DESC(CROS_EC_SECTION ":" CROS_EC_VERSION32),
	[USB_STR_I2C_NAME]     = USB_STRING_DESC("I2C"),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * Support I2C bridging over USB.
 */

#ifdef SECTION_IS_RW

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"touchpad", I2C_PORT_TOUCHPAD, 400,
		GPIO_TOUCHPAD_I2C_SCL, GPIO_TOUCHPAD_I2C_SDA},
#ifdef BOARD_WAND
	{"charger", I2C_PORT_CHARGER, 100,
		GPIO_CHARGER_I2C_SCL, GPIO_CHARGER_I2C_SDA},
#endif
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef BOARD_STAFF
#define KBLIGHT_PWM_FREQ 100 /* Hz */
#else
#define KBLIGHT_PWM_FREQ 10000 /* Hz */
#endif

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(TIM_KBLIGHT), STM32_TIM_CH(1), 0, KBLIGHT_PWM_FREQ},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

int usb_i2c_board_is_enabled(void)
{
	/* Disable I2C passthrough when the system is locked */
	return !system_is_locked();
}

#ifdef CONFIG_KEYBOARD_BOARD_CONFIG
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 50,
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x3c, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};
#endif
#endif

#if defined(BOARD_WAND) && defined(SECTION_IS_RW)
struct consumer const ec_ec_usart_consumer;
static struct usart_config const ec_ec_usart;

struct queue const ec_ec_comm_slave_input = QUEUE_DIRECT(64, uint8_t,
				ec_ec_usart.producer, ec_ec_usart_consumer);
struct queue const ec_ec_comm_slave_output = QUEUE_DIRECT(64, uint8_t,
				null_producer, ec_ec_usart.consumer);

struct consumer const ec_ec_usart_consumer = {
	.queue = &ec_ec_comm_slave_input,
	.ops   = &((struct consumer_ops const) {
		.written = ec_ec_comm_slave_written,
	}),
};

static struct usart_config const ec_ec_usart =
	USART_CONFIG(EC_EC_UART,
		usart_rx_interrupt,
		usart_tx_interrupt,
		115200,
		USART_CONFIG_FLAG_HDSEL,
		ec_ec_comm_slave_input,
		ec_ec_comm_slave_output);
#endif /* BOARD_WAND && SECTION_IS_RW */

/******************************************************************************
 * Initialize board.
 */
static int has_keyboard_backlight;

static void board_init(void)
{
	/* Detect keyboard backlight: pull-down means it is present. */
	has_keyboard_backlight = !gpio_get_level(GPIO_KEYBOARD_BACKLIGHT);

	CPRINTS("Backlight%s present", has_keyboard_backlight ? "" : " not");

#ifdef BOARD_STAFF
	if (!has_keyboard_backlight) {
		/*
		 * Earlier staff boards have both PU and PD stuffed, and end up
		 * being detected as not have keyboard backlight. However, we
		 * need to enable internal PD on the pin, otherwise backlight
		 * will always be on.
		 * TODO(b:67722756): Remove this hack when old boards are
		 * deprecated.
		 */
		gpio_set_flags(GPIO_KEYBOARD_BACKLIGHT,
			       GPIO_PULL_DOWN | GPIO_INPUT);
	}
#endif /* BOARD_STAFF */

#if defined(BOARD_WAND) && defined(SECTION_IS_RW)
	/* USB to serial queues */
	queue_init(&ec_ec_comm_slave_input);
	queue_init(&ec_ec_comm_slave_output);

	/* UART init */
	usart_init(&ec_ec_usart);
#endif
}
/* This needs to happen before PWM is initialized. */
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_INIT_PWM - 1);

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/* Remap USART DMA to match the USART driver */
	/*
	 * the DMA mapping is :
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 */
	STM32_SYSCFG_CFGR1 |= (1 << 9) | (1 << 10); /* Remap USART1 RX/TX DMA */
}

int board_has_keyboard_backlight(void)
{
	return has_keyboard_backlight;
}

/*
 * Side-band USB wake, to be able to wake lid even in deep S3, when USB
 * controller is off.
 */
void board_usb_wake(void)
{
#ifdef BOARD_WAND
	/* FIXME: Implement side-band wake for wand. */
#else
	/*
	 * Poke detection pin for about 500us, we disable interrupts
	 * to make sure that we do not get preempted (setting GPIO high
	 * for too long would prevent pulse detection on lid EC side from
	 * working properly, or even kill hammer power if it is held for
	 * longer than debounce time).
	 */
	interrupt_disable();
	gpio_set_flags(GPIO_BASE_DET, GPIO_OUT_HIGH);
	udelay(500);
	gpio_set_flags(GPIO_BASE_DET, GPIO_INPUT);
	interrupt_enable();
#endif
}

/*
 * Get entropy based on Clock Recovery System, which is enabled on hammer to
 * synchronize USB SOF with internal oscillator.
 */
int board_get_entropy(void *buffer, int len)
{
	int i = 0;
	uint8_t *data = buffer;
	uint32_t start;
	/* We expect one SOF per ms, so wait at most 2ms. */
	const uint32_t timeout = 2*MSEC;

	for (i = 0; i < len; i++) {
		STM32_CRS_ICR |= STM32_CRS_ICR_SYNCOKC;
		start = __hw_clock_source_read();
		while (!(STM32_CRS_ISR & STM32_CRS_ISR_SYNCOKF)) {
			if ((__hw_clock_source_read() - start) > timeout)
				return 0;
			usleep(500);
		}
		/* Pick 8 bits, including FEDIR and 7 LSB of FECAP. */
		data[i] = STM32_CRS_ISR >> 15;
	}

	return 1;
}

/*
 * Generate a USB serial number from unique chip ID.
 */
const char *board_read_serial(void)
{
	static char str[CONFIG_SERIALNO_LEN];

	if (str[0] == '\0') {
		uint8_t *id;
		int pos = 0;
		int idlen = system_get_chip_unique_id(&id);
		int i;

		for (i = 0; i < idlen && pos < sizeof(str); i++, pos += 2) {
			snprintf(&str[pos], sizeof(str)-pos,
				"%02x", id[i]);
		}
	}

	return str;
}

int board_write_serial(const char *serialno)
{
	return 0;
}

#ifdef BOARD_WAND
/* TODO(b:66575472): This assumes external power is always present. */
int extpower_is_present(void)
{
	return 1;
}
#endif
