/*
 * bme_driver.h
 *
 *  Created on: 3 мар. 2021 г.
 *      Author: alexm
 */

#ifndef INC_BME_DRIVER_H_
#define INC_BME_DRIVER_H_

struct _weather_data_t {
	int temperature;
	int pressure;
	int humidity;
};
typedef struct _weather_data_t weather_data_t;

void bme_driver_test(void);
int bme_driver_init(void);
int bme_driver_get_weather(weather_data_t *out);

#endif /* INC_BME_DRIVER_H_ */
