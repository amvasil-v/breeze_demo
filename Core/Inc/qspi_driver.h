/*
 * qspi_driver.h
 *
 *  Created on: Feb 22, 2021
 *      Author: alexm
 */

#ifndef INC_QSPI_DRIVER_H_
#define INC_QSPI_DRIVER_H_

#include "main.h"

#define QSPI_PAGE_SIZE                  256

int QSPI_init_device(void);
int QSPI_sector_erase(uint32_t address);
int QSPI_chip_erase(void);
int QSPI_area_erase(uint32_t start_addr, size_t size);
int QSPI_write_page(uint8_t *buf, size_t size, uint32_t dst);
int QSPI_write_pattern(uint32_t start_addr, size_t len);
int QSPI_read_page(uint8_t *dst, size_t size, uint32_t addr);
int QSPI_verify_pattern(uint32_t start_addr, size_t len);


#endif /* INC_QSPI_DRIVER_H_ */
