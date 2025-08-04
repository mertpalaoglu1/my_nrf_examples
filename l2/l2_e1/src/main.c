/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

/* STEP 7 - Change the sleep time from 1000 ms to 100 ms */
#define SLEEP_TIME_MS 100

/* STEP 3.1 - Get the node identifier for button 1 through its alias sw0 */
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
/* STEP 3.2 - Get the device pointer, pin number, and pin's configuration flags through gpio_dt_spec*/
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(SW1_NODE,gpios);
/* LED0_NODE is the devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)


static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
int main(void)
{
	int ret;

	if (!device_is_ready(led.port)) {
		return -1;
	}
	if (!device_is_ready(led1.port)) {
		return -1;
	}
	/* STEP 4 - Verify that the device is ready for use */
	if (!device_is_ready(button.port)) {
	return -1;
	}
	if (!device_is_ready(button1.port)){
		return -1;
	}
	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (!device_is_ready(led.port)) {
		return -1;
	}

	/* STEP 5 - Configure the pin connected to the button to be an input pin and set its
	 * hardware specifications */
	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
 		return -1;
	}
	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret < 0) {
 		return -1;
	}
	while (1) {
		/* STEP 6.1 - Read the status of the button and store it */

		bool val = gpio_pin_get_dt(&button); //buton degerini aldık.
		bool val1= gpio_pin_get_dt(&button1);
		/* STEP 6.2 - Update the LED to the status of the button */

		gpio_pin_set_dt(&led,val); //O degeri 0-1 lede atadık.
		gpio_pin_set_dt(&led1,val1);
		
		k_msleep(SLEEP_TIME_MS); // Put the main thread to sleep for 100ms for power optimization
	}
}