/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Nov 5, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <dfu/cs_FirmwareSections.h>
#include <logging/cs_Logger.h>

// -------------- callback for F Storage --------------

void firmwareReaderFsEventHandler(nrf_fstorage_evt_t * p_evt)  {
	LOGd("firmware reader fs event handler called");
}


// specializations computing the address values of the various firmware sections

template<>
const FirmwareSectionLocation getFirmwareSectionLocation<FirmwareSection::Mbr>() {
//extern int __start_mbr_params_page, __stop_mbr_params_page;
//static const uint32_t start_mbr_params_page = static_cast<uint32_t>(__start_mbr_params_page);
//static const uint32_t stop_mbr_params_page = static_cast<uint32_t>(__stop_mbr_params_page);
	return {0x00000000,
			0x00001000};
}

template<>
const FirmwareSectionLocation getFirmwareSectionLocation<FirmwareSection::Bluenet>() {
	return {g_APPLICATION_START_ADDRESS,
			g_APPLICATION_START_ADDRESS + g_APPLICATION_LENGTH};
}

template<>
const FirmwareSectionLocation getFirmwareSectionLocation<FirmwareSection::Microapp>() {
	return {g_FLASH_MICROAPP_BASE,
			g_FLASH_MICROAPP_BASE + static_cast<uint32_t>((CS_FLASH_PAGE_SIZE * g_FLASH_MICROAPP_PAGES) - 1)};
}

template<>
const FirmwareSectionLocation getFirmwareSectionLocation<FirmwareSection::Bootloader>() {
	return {0x00071000,
			0x00071000 + 0x0000D000};
}




// --------------------------------------------------------------------------------
// -------------------- Memory region definitions for fstorage --------------------
// --------------------------------------------------------------------------------

// ------- Bluenet -------
NRF_FSTORAGE_DEF(nrf_fstorage_t firmwareReaderFsInstanceBluenet) = {
		.evt_handler = firmwareReaderFsEventHandler,
		.start_addr  = getFirmwareSectionLocation<FirmwareSection::Bluenet>()._start,
		.end_addr    = getFirmwareSectionLocation<FirmwareSection::Bluenet>()._end,
};

// ------- Bootloader -------
NRF_FSTORAGE_DEF(nrf_fstorage_t firmwareReaderFsInstanceBootloader) = {
		.evt_handler = firmwareReaderFsEventHandler,
		.start_addr  = getFirmwareSectionLocation<FirmwareSection::Bootloader>()._start,
		.end_addr    = getFirmwareSectionLocation<FirmwareSection::Bootloader>()._end,
};

// ------- MBR -------

NRF_FSTORAGE_DEF(nrf_fstorage_t firmwareReaderFsInstanceMbr) = {
		.evt_handler = firmwareReaderFsEventHandler,
		.start_addr  = getFirmwareSectionLocation<FirmwareSection::Mbr>()._start,
		.end_addr    = getFirmwareSectionLocation<FirmwareSection::Mbr>()._end,
};


// specializations that implement getFirmwareSectionInfo

template<>
const FirmwareSectionInfo getFirmwareSectionInfo<FirmwareSection::Bluenet>() {
	return FirmwareSectionInfo(&firmwareReaderFsInstanceBluenet,
			getFirmwareSectionLocation<FirmwareSection::Bluenet>());
}

template<>
const FirmwareSectionInfo getFirmwareSectionInfo<FirmwareSection::Bootloader>() {
	return FirmwareSectionInfo{._fStoragePtr = &firmwareReaderFsInstanceBootloader,
			._addr        = getFirmwareSectionLocation<FirmwareSection::Bootloader>()};
}

template<>
const FirmwareSectionInfo getFirmwareSectionInfo<FirmwareSection::Mbr>() {
	return FirmwareSectionInfo{._fStoragePtr = &firmwareReaderFsInstanceMbr,
			._addr        = getFirmwareSectionLocation<FirmwareSection::Mbr>()};
}
