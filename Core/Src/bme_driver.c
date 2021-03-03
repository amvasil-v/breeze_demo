/*
 * bme_driver.c
 *
 *  Created on: 3 мар. 2021 г.
 *      Author: alexm
 *
 *  Based on: https://github.com/ProjectsByJRP/stm32-bme280#readme
 */

#include "bme_driver.h"

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include "main.h"

#include "BME280/bme280.h"

extern I2C_HandleTypeDef hi2c1;

extern void DEV_Delay_ms_impl(uint32_t ms);

static void user_delay_us(uint32_t period, void *intf_ptr)
{
	/*
	 * Return control or wait,
	 * for a period amount of milliseconds
	 */
	(void)intf_ptr;
	DEV_Delay_ms_impl((period + 999) / 1000);
}

static int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
	/*
	 * The parameter intf_ptr can be used as a variable to store the I2C address of the device
	 */

	/*
	 * Data on the bus should be like
	 * |------------+---------------------|
	 * | I2C action | Data                |
	 * |------------+---------------------|
	 * | Start      | -                   |
	 * | Write      | (reg_addr)          |
	 * | Stop       | -                   |
	 * | Start      | -                   |
	 * | Read       | (reg_data[0])       |
	 * | Read       | (....)              |
	 * | Read       | (reg_data[len - 1]) |
	 * | Stop       | -                   |
	 * |------------+---------------------|
	 */

	uint16_t dev_addr = BME280_I2C_ADDR_PRIM;
	while (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(dev_addr<<1), 3, 100) != HAL_OK);

	if (HAL_I2C_Mem_Read(&hi2c1,		// i2c handle
			(uint8_t)(dev_addr<<1),		// i2c address, left aligned
			(uint8_t)reg_addr,			// register address
			I2C_MEMADD_SIZE_8BIT,		// bme280 uses 8bit register addresses
			reg_data,					// write returned data to this variable
			len,						// how many bytes to expect returned
			100) != HAL_OK) {
		return -1;
	}
	return 0;
}

static int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
	/*
	 * The parameter intf_ptr can be used as a variable to store the I2C address of the device
	 */

	/*
	 * Data on the bus should be like
	 * |------------+---------------------|
	 * | I2C action | Data                |
	 * |------------+---------------------|
	 * | Start      | -                   |
	 * | Write      | (reg_addr)          |
	 * | Write      | (reg_data[0])       |
	 * | Write      | (....)              |
	 * | Write      | (reg_data[len - 1]) |
	 * | Stop       | -                   |
	 * |------------+---------------------|
	 */
	uint16_t dev_addr = BME280_I2C_ADDR_PRIM;
	while (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(dev_addr<<1), 3, 100) != HAL_OK);

	if (HAL_I2C_Mem_Write(&hi2c1,			// i2c handle
			(uint8_t)(dev_addr<<1),			// i2c address, left aligned
			(uint8_t)reg_addr,				// register address
			I2C_MEMADD_SIZE_8BIT,			// bme280 uses 8bit register addresses
			(uint8_t *)reg_data,						// write returned data to reg_data
			len,							// write how many bytes
			100) != HAL_OK) {
		return -1;
	}

	return 0;
}

static int bme_driver_init_dev(struct bme280_dev *dev)
{
	int8_t rslt = BME280_OK;
	uint8_t dev_addr = BME280_I2C_ADDR_PRIM;

	dev->intf_ptr = &dev_addr;
	dev->intf = BME280_I2C_INTF;
	dev->read = user_i2c_read;
	dev->write = user_i2c_write;
	dev->delay_us = user_delay_us;

	rslt = bme280_init(dev);

	return rslt;
}

static void print_sensor_data(struct bme280_data *comp_data)
{
#ifdef BME280_FLOAT_ENABLE
	printf("%0.2f, %0.2f, %0.2f\r\n",comp_data->temperature, comp_data->pressure, comp_data->humidity);
#else
	int temp = comp_data->temperature;
	int pres = comp_data->pressure / 133;
	int hum = comp_data->humidity / 1024;
	printf("%d.%02d C, %d mmHg, %d %%\r\n",temp / 100, temp % 100, pres, hum);
#endif
}

int8_t stream_sensor_data_forced_mode(struct bme280_dev *dev)
{
	int8_t rslt;
	uint8_t settings_sel;
	uint32_t req_delay;
	struct bme280_data comp_data;

	/* Recommended mode of operation: Indoor navigation */
	dev->settings.osr_h = BME280_OVERSAMPLING_1X;
	dev->settings.osr_p = BME280_OVERSAMPLING_16X;
	dev->settings.osr_t = BME280_OVERSAMPLING_2X;
	dev->settings.filter = BME280_FILTER_COEFF_16;

	settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

	rslt = bme280_set_sensor_settings(settings_sel, dev);

	/*Calculate the minimum delay required between consecutive measurement based upon the sensor enabled
	 *  and the oversampling configuration. */
	req_delay = bme280_cal_meas_delay(&dev->settings);

	printf("Temperature, Pressure, Humidity\r\n");
	/* Continuously stream sensor data */
	while (1) {
		rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
		/* Wait for the measurement to complete and print data @25Hz */
		dev->delay_us(req_delay * 1000, dev->intf_ptr);
		rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
		print_sensor_data(&comp_data);
		DEV_Delay_ms_impl(2000);
	}
	return rslt;
}



void bme_driver_test(void)
{
	struct bme280_dev dev;

	if (bme_driver_init_dev(&dev)) {
		printf("Failed to init BME280\r\n");
		Error_Handler();
	} else {
		printf("Done init BME280\r\n");
	}

	led_set(1, 1);

	if (stream_sensor_data_forced_mode(&dev)) {
		Error_Handler();
	}
}

