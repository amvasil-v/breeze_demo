/*
 * qspi_driver.c
 *
 *  Created on: Feb 22, 2021
 *      Author: alexm
 */

#include "qspi_driver.h"

#include <stdio.h>
#include <string.h>

extern QSPI_HandleTypeDef hqspi;
extern DMA_HandleTypeDef hdma_quadspi;

#define QSPI_DEVICE_ID		0x15EF

#define QSPI_TEST_PATTERN_INC			3
#define QSPI_TEST_PATTERN_READ_SIZE		100
#define QSPI_ERASE_SECTOR_SIZE			4096

#define LED_BLINK_PERIOD				5
#define LED_BLINK_COND(X) 				(!(X++ % LED_BLINK_PERIOD))
#define LED_BLINK_HELPER(CNT, LED)		do {if(LED_BLINK_COND(CNT)){led_blink(LED);}} while(0)

/* QSPI commands */
#define QSPI_READ_MAN_DEVICE_ID_CMD		0x90
#define QSPI_RESET_ENABLE_CMD			0x66
#define QSPI_RESET_MEMORY_CMD			0x99
#define QSPI_SECTOR_ERASE_CMD			0x20
#define QSPI_CHIP_ERASE_CMD				0xC7
#define QSPI_PAGE_PROG_CMD              0x02
#define QSPI_FAST_READ_CMD              0x0B
#define QSPI_WRITE_ENABLE_CMD           0x06
#define QSPI_READ_STATUS_REG_CMD        0x05
#define QSPI_READ_VOL_CFG_REG_CMD       0x85
#define QSPI_WRITE_VOL_CFG_REG_CMD      0x81

#define QSPI_DUMMY_CLOCK_CYCLES_READ    8

uint8_t QSPI_ResetMemory(QSPI_HandleTypeDef *hqspi);
uint8_t QSPI_EnterFourBytesAddress(QSPI_HandleTypeDef *hqspi);
void QSPI_WriteEnable(QSPI_HandleTypeDef *hqspi);
void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *hqspi, uint32_t timeout);
void QSPI_DummyCyclesCfg(QSPI_HandleTypeDef *hqspi);
int QSPI_test(void);

static void QSPI_command_init(QSPI_CommandTypeDef *cmd)
{
	cmd->InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	cmd->AddressSize       = QSPI_ADDRESS_24_BITS;
	cmd->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd->DdrMode           = QSPI_DDR_MODE_DISABLE;
	cmd->DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd->SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
	cmd->AddressMode = QSPI_ADDRESS_1_LINE;
	cmd->Address     = 0;
	cmd->DataMode    = QSPI_DATA_NONE;
	cmd->DummyCycles = 0;
	cmd->NbData      = 0;
}

int QSPI_init_device(void)
{
	QSPI_CommandTypeDef sCommand;
	uint16_t device_id = 0;

	printf("Initialize QSPI flash\r\n");

	if (QSPI_ResetMemory(&hqspi) != HAL_OK)
	{
		Error_Handler();
	}
	HAL_Delay(500);
	printf("QSPI reset done\r\n");

	QSPI_command_init(&sCommand);
	sCommand.Instruction = QSPI_READ_MAN_DEVICE_ID_CMD;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.NbData      = sizeof(device_id);

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&device_id, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Read device ID: 0x%04x\r\n", device_id);

	if (device_id == QSPI_DEVICE_ID) {
		return 0;
	} else {
		return -1;
	}
}

int QSPI_sector_erase(uint32_t address)
{
	QSPI_CommandTypeDef sCommand;

	//printf("QSPI 4k sector erase at 0x%08lx\r\n", address);
	/* Enable write operations ------------------------------------------- */
	QSPI_WriteEnable(&hqspi);
	//printf("QSPI write enabled\r\n");
	/* Erasing Sequence -------------------------------------------------- */
	QSPI_command_init(&sCommand);
	sCommand.Instruction = QSPI_SECTOR_ERASE_CMD;
	sCommand.Address     = address;
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}
	//printf("Waiting for erase complete\r\n");
	/* Configure automatic polling mode to wait for end of erase ------- */
	QSPI_AutoPollingMemReady(&hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
	HAL_Delay(100);
	//printf("QSPI erase done\r\n");
	return 0;
}

int QSPI_chip_erase(void)
{
	QSPI_CommandTypeDef sCommand;

	printf("QSPI chip erase\r\n");
	/* Enable write operations ------------------------------------------- */
	QSPI_WriteEnable(&hqspi);
	printf("QSPI write enabled\r\n");
	/* Erasing Sequence -------------------------------------------------- */
	QSPI_command_init(&sCommand);
	sCommand.Instruction = QSPI_CHIP_ERASE_CMD;
	sCommand.AddressMode = QSPI_ADDRESS_NONE;
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}
	printf("Waiting for erase complete\r\n");
	/* Configure automatic polling mode to wait for end of erase ------- */
	uint32_t start_tick = HAL_GetTick();
	QSPI_AutoPollingMemReady(&hqspi, 20000);
	uint32_t elapsed = HAL_GetTick() - start_tick;
	printf("Wait elapsed %lu ms\r\n", elapsed);
	if (elapsed < 1000) {
		printf("Wait failed, wait 10 sec\r\n");
		HAL_Delay(10000);
		return -1;
	}
	HAL_Delay(100);
	printf("QSPI erase done\r\n");
	return 0;
}

int QSPI_area_erase(uint32_t start_addr, size_t size)
{
	printf("QSPI area erase\r\n");
	for (uint32_t addr = start_addr & QSPI_ERASE_SECTOR_SIZE; addr < start_addr + size + QSPI_ERASE_SECTOR_SIZE; addr += QSPI_ERASE_SECTOR_SIZE)
	{
		if(QSPI_sector_erase(addr)) {
			return -1;
		}
		led_blink(0);
	}
	printf("QSPI area 0x%08lx, size %u erased\r\n", start_addr, size);
	return 0;
}

int QSPI_write_page(uint8_t *buf, size_t size, uint32_t dst)
{
	QSPI_CommandTypeDef sCommand;

	if (size > QSPI_PAGE_SIZE) {
		printf("QSPI_write_page() size must be not greater than page - 256 bytes\r\n");
		return -1;
	}

	//printf("QSPI write at 0x%08lx %u bytes\r\n", dst, size);
	/* Enable write operations ------------------------------------------- */
	QSPI_WriteEnable(&hqspi);
	//printf("QSPI write enabled\r\n");

	/* Writing Sequence ------------------------------------------------ */
	QSPI_command_init(&sCommand);
	sCommand.Instruction = QSPI_PAGE_PROG_CMD;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.Address     = dst;
	sCommand.NbData      = size;

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Transmit(&hqspi, buf, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	//printf("Waiting for write complete\r\n");
	/* Configure automatic polling mode to wait for end of program ----- */
	QSPI_AutoPollingMemReady(&hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
	HAL_Delay(10);
	//printf("QSPI write done\r\n");

	return 0;
}

int QSPI_write_pattern(uint32_t start_addr, size_t len)
{
	size_t to_write = len;
	static uint8_t buf[QSPI_PAGE_SIZE];
	uint8_t pattern = 0x01;
	uint32_t addr = start_addr;
	uint8_t led_cnt = 0;

	while (to_write) {
		size_t bytes_to_write = to_write > QSPI_PAGE_SIZE ? QSPI_PAGE_SIZE : to_write;
		for (int i = 0; i < bytes_to_write; i++) {
			buf[i] = pattern;
			pattern += QSPI_TEST_PATTERN_INC;
		}
		if (QSPI_write_page(buf, bytes_to_write, addr)) {
			return -1;
		}
		addr += bytes_to_write;
		to_write -= bytes_to_write;
		LED_BLINK_HELPER(led_cnt, 0);
	}
	printf("Pattern written at 0x%08lx, length %u\r\n", start_addr, len);
	return 0;
}

int QSPI_read_page(uint8_t *dst, size_t size, uint32_t addr)
{
	QSPI_CommandTypeDef sCommand;

	if (size > QSPI_PAGE_SIZE) {
		printf("QSPI_read_page() size must be not greater than page - 256 bytes\r\n");
		return -1;
	}
	QSPI_command_init(&sCommand);
	//printf("Reading from QSPI at 0x%08lx, length %u\r\n", addr, size);
	/* Configure Volatile Configuration register (with new dummy cycles) */
	QSPI_DummyCyclesCfg(&hqspi);
	/* Reading Sequence ------------------------------------------------ */
	sCommand.Instruction = QSPI_FAST_READ_CMD;
	sCommand.DummyCycles = QSPI_DUMMY_CLOCK_CYCLES_READ;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.Address     = addr;
	sCommand.NbData      = size;
	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_QSPI_Receive(&hqspi, dst, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}
	HAL_Delay(2);
	//printf("Read complete\r\n");
	return 0;
}

int QSPI_verify_pattern(uint32_t start_addr, size_t len)
{
	size_t to_read = len;
	static uint8_t buf[QSPI_PAGE_SIZE];
	uint8_t pattern = 0x01;
	uint32_t addr = start_addr;
	uint8_t led_cnt = 0;

	memset(buf, 0xAA, QSPI_PAGE_SIZE);

	while (to_read) {
		size_t bytes_to_read = to_read > QSPI_TEST_PATTERN_READ_SIZE ? QSPI_TEST_PATTERN_READ_SIZE : to_read;
		if (QSPI_read_page(buf, bytes_to_read, addr)) {
			return -1;
		}
		for (int i = 0; i < bytes_to_read; i++) {
			if (buf[i] != pattern) {
				printf("Pattern mismatch at 0x%08lx: expected 0x%02x, read 0x%02x\r\n",
						addr + i, pattern, buf[i]);
				return -1;
			}
			pattern += QSPI_TEST_PATTERN_INC;
		}
		addr += bytes_to_read;
		to_read -= bytes_to_read;
		LED_BLINK_HELPER(led_cnt, 0);
	}
	printf("Pattern verified at 0x%08lx, length %u\r\n", start_addr, len);
	return 0;
}

int QSPI_test(void)
{
	QSPI_CommandTypeDef sCommand;
	uint32_t address = 0;

	printf("Start QSPI init\r\n");

	/* QSPI memory reset */
	if (QSPI_ResetMemory(&hqspi) != HAL_OK)
	{
		Error_Handler();
	}

	HAL_Delay(100);

	printf("QSPI reset done\r\n");

	/* Set the QSPI memory in 4-bytes address mode */
#if ENTER_4BYTE_ADDRESS_MODE
	if (QSPI_EnterFourBytesAddress(&hqspi) != HAL_OK)
	{
		Error_Handler();
	}
	HAL_Delay(100);
#endif

	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	sCommand.AddressSize       = QSPI_ADDRESS_24_BITS;
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

	/* Request device ID */
	uint16_t device_id = 0;

	printf("QSPI read device ID\r\n");

	sCommand.Instruction = QSPI_READ_MAN_DEVICE_ID_CMD;
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
	sCommand.Address     = 0;
	sCommand.DataMode    = QSPI_DATA_NONE;
	sCommand.DummyCycles = 0;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.NbData      = sizeof(device_id);

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&device_id, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Read device ID: 0x%04x\r\n", device_id);

#if 0
	/* Request JEDEC device ID */
	uint32_t jedec_id = 0xAAAAAAAA;

	printf("QSPI read JEDEC device ID\r\n");

	sCommand.Instruction = READ_JEDEC_DEVICE_ID;
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
	sCommand.Address     = 0;
	sCommand.DataMode    = QSPI_DATA_NONE;
	sCommand.DummyCycles = 0;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.NbData      = 4;

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&jedec_id, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Read JEDEC ID: 0x%08lx\r\n", jedec_id);
#endif


	/* Enable write operations ------------------------------------------- */
	QSPI_WriteEnable(&hqspi);

	printf("QSPI write enabled\r\n");

	/* Erasing Sequence -------------------------------------------------- */
	sCommand.Instruction = QSPI_SECTOR_ERASE_CMD;
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
	sCommand.Address     = address;
	sCommand.DataMode    = QSPI_DATA_NONE;
	sCommand.DummyCycles = 0;

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Waiting for erase complete\r\n");
	/* Configure automatic polling mode to wait for end of erase ------- */
	QSPI_AutoPollingMemReady(&hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
	printf("QSPI erase done\r\n");

	/* Enable write operations ----------------------------------------- */
	QSPI_WriteEnable(&hqspi);

	printf("Writing to QSPI\r\n");

	uint8_t test_write_buf[QSPI_PAGE_SIZE];
	for (int i = 0; i < sizeof(test_write_buf); i++) {
		test_write_buf[i] = i + 5;
	}

	/* Writing Sequence ------------------------------------------------ */
	sCommand.Instruction = QSPI_PAGE_PROG_CMD;
	sCommand.AddressMode = QSPI_ADDRESS_1_LINE;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.Address     = address;
	sCommand.NbData      = sizeof(test_write_buf);

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("PAGE_PROG_CMD cmd failed\r\n");
		Error_Handler();
	}

	if (HAL_QSPI_Transmit(&hqspi, test_write_buf, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("PAGE_PROG_CMD data failed\r\n");
		Error_Handler();
	}

	printf("Waiting for write complete\r\n");
	/* Configure automatic polling mode to wait for end of program ----- */
	QSPI_AutoPollingMemReady(&hqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE);
	printf("QSPI write done\r\n");

	HAL_Delay(100);


#define QSPI_READ_BUF_SIZE		5
	uint8_t test_read_buf[QSPI_READ_BUF_SIZE];
	memset(test_read_buf, 0, sizeof(test_read_buf));

	printf("Reading from QSPI\r\n");
	/* Configure Volatile Configuration register (with new dummy cycles) */
	QSPI_DummyCyclesCfg(&hqspi);

	/* Reading Sequence ------------------------------------------------ */
	sCommand.Instruction = QSPI_FAST_READ_CMD;
	sCommand.DummyCycles = QSPI_DUMMY_CLOCK_CYCLES_READ;
	sCommand.DataMode    = QSPI_DATA_1_LINE;
	sCommand.Address     = address;
	sCommand.NbData      = QSPI_READ_BUF_SIZE;

	if (HAL_QSPI_Command(&hqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Receive(&hqspi, test_read_buf, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	printf("Read complete\r\nRead: ");

	for (int i = 0; i < QSPI_READ_BUF_SIZE; i++) {
		printf("0x%02x ", test_read_buf[i]);
	}

	printf("\r\n");
	printf("QSPI test done\r\n");

	return 0;
}

/**
 * @brief  This function reset the QSPI memory.
 * @param  hqspi: QSPI handle
 * @retval None
 */
uint8_t QSPI_ResetMemory(QSPI_HandleTypeDef *phqspi)
{
	QSPI_CommandTypeDef s_command;

	/* Initialize the Mode Bit Reset command */
	s_command.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	s_command.Instruction       = QSPI_RESET_ENABLE_CMD;
	s_command.AddressMode       = QSPI_ADDRESS_NONE;
	s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	s_command.DataMode          = QSPI_DATA_NONE;
	s_command.DummyCycles       = 0;
	s_command.DdrMode           = QSPI_DDR_MODE_DISABLE;
	s_command.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	s_command.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

	/* Send the command */
	if (HAL_QSPI_Command(phqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		return HAL_ERROR;
	}

	/* Send the SW reset command */
	s_command.Instruction       = QSPI_RESET_MEMORY_CMD;
	if (HAL_QSPI_Command(phqspi, &s_command, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		return HAL_ERROR;
	}

	/* Configure automatic polling mode to wait the memory is ready */
	QSPI_AutoPollingMemReady(phqspi, HAL_QPSI_TIMEOUT_DEFAULT_VALUE);

	return HAL_OK;
}

/**
 * @brief  This function send a Write Enable and wait it is effective.
 * @param  hqspi: QSPI handle
 * @retval None
 */
void QSPI_WriteEnable(QSPI_HandleTypeDef *phqspi)
{
	QSPI_CommandTypeDef     sCommand;
	QSPI_AutoPollingTypeDef sConfig;

	/* Enable write operations ------------------------------------------ */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	sCommand.Instruction       = QSPI_WRITE_ENABLE_CMD;
	sCommand.AddressMode       = QSPI_ADDRESS_NONE;
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	sCommand.DataMode          = QSPI_DATA_NONE;
	sCommand.DummyCycles       = 0;
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

	if (HAL_QSPI_Command(phqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("WRITE_ENABLE_CMD cmd failed\r\n");
		Error_Handler();
	}

	/* Configure automatic polling mode to wait for write enabling ---- */
	sConfig.Match           = 0x02;
	sConfig.Mask            = 0x02;
	sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
	sConfig.StatusBytesSize = 1;
	sConfig.Interval        = 0x10;
	sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

	sCommand.Instruction    = QSPI_READ_STATUS_REG_CMD;
	sCommand.DataMode       = QSPI_DATA_1_LINE;

	if (HAL_QSPI_AutoPolling(phqspi, &sCommand, &sConfig, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		printf("WRITE_ENABLE_CMD poll failed\r\n");
		Error_Handler();
	}
}

/**
 * @brief  This function read the SR of the memory and wait the EOP.
 * @param  hqspi: QSPI handle
 * @retval None
 */
void QSPI_AutoPollingMemReady(QSPI_HandleTypeDef *phqspi, uint32_t timeout)
{
	QSPI_CommandTypeDef     sCommand;
	QSPI_AutoPollingTypeDef sConfig;

	/* Configure automatic polling mode to wait for memory ready ------ */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	sCommand.Instruction       = QSPI_READ_STATUS_REG_CMD;
	sCommand.AddressMode       = QSPI_ADDRESS_NONE;
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	sCommand.DataMode          = QSPI_DATA_1_LINE;
	sCommand.DummyCycles       = 0;
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

	sConfig.Match           = 0x00;
	sConfig.Mask            = 0x01;
	sConfig.MatchMode       = QSPI_MATCH_MODE_AND;
	sConfig.StatusBytesSize = 1;
	sConfig.Interval        = 0x10;
	sConfig.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

	if (HAL_QSPI_AutoPolling(phqspi, &sCommand, &sConfig, timeout) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief  This function configure the dummy cycles on memory side.
 * @param  hqspi: QSPI handle
 * @retval None
 */
void QSPI_DummyCyclesCfg(QSPI_HandleTypeDef *phqspi)
{
	QSPI_CommandTypeDef sCommand;
	uint8_t reg;

	/* Read Volatile Configuration register --------------------------- */
	sCommand.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
	sCommand.Instruction       = QSPI_READ_VOL_CFG_REG_CMD;
	sCommand.AddressMode       = QSPI_ADDRESS_NONE;
	sCommand.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	sCommand.DataMode          = QSPI_DATA_1_LINE;
	sCommand.DummyCycles       = 0;
	sCommand.DdrMode           = QSPI_DDR_MODE_DISABLE;
	sCommand.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
	sCommand.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;
	sCommand.NbData            = 1;

	if (HAL_QSPI_Command(phqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Receive(phqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	/* Enable write operations ---------------------------------------- */
	QSPI_WriteEnable(phqspi);

	/* Write Volatile Configuration register (with new dummy cycles) -- */
	sCommand.Instruction = QSPI_WRITE_VOL_CFG_REG_CMD;
	MODIFY_REG(reg, 0xF0, (QSPI_DUMMY_CLOCK_CYCLES_READ << POSITION_VAL(0xF0)));

	if (HAL_QSPI_Command(phqspi, &sCommand, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}

	if (HAL_QSPI_Transmit(phqspi, &reg, HAL_QPSI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
	{
		Error_Handler();
	}
}

