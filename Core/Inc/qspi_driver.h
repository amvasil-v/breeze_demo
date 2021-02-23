/*
 * qspi_driver.h
 *
 *  Created on: Feb 22, 2021
 *      Author: alexm
 */

#ifndef INC_QSPI_DRIVER_H_
#define INC_QSPI_DRIVER_H_

#include "main.h"

uint8_t QSPI_ResetMemory(QSPI_HandleTypeDef *hqspi);
uint8_t QSPI_EnterFourBytesAddress(QSPI_HandleTypeDef *hqspi);
void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi);
void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi, uint32_t timeout);
void QSPI_DummyCyclesCfg(QSPI_HandleTypeDef *hqspi);

int QSPI_test(void);

int QSPI_init_device(void);
int QSPI_sector_erase(uint32_t address);
int QSPI_chip_erase(void);
int QSPI_area_erase(uint32_t start_addr, size_t size);
int QSPI_write_page(uint8_t *buf, size_t size, uint32_t dst);
int QSPI_write_pattern(uint32_t start_addr, size_t len);
int QSPI_read_page(uint8_t *dst, size_t size, uint32_t addr);
int QSPI_verify_pattern(uint32_t start_addr, size_t len);


#endif /* INC_QSPI_DRIVER_H_ */
