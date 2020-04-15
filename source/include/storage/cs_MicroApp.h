#pragma once

#include <events/cs_EventListener.h>

enum class MICROAPP_VALIDATION { NONE, CHECKSUM, BOOTS, FAILS };

/**
 * The class MicroApp has functionality to store a second app (and perhaps in the future even more apps) on another
 * part of the flash memory.
 */
class MicroApp: public EventListener {
	private:
		/**
		 * Singleton, constructor, also copy constructor, is private.
		 */
		MicroApp();
		MicroApp(MicroApp const&);
		void operator=(MicroApp const &);

	protected:
		/**
		 * Write a chunk to flash memory. Writes to buffer at index start + index * CHUNK_SIZE up to size of the data.
		 */
		int writeChunk(uint8_t index, uint8_t *data, uint8_t size);

		/**
		 * Erases all pages of the MicroApp binary.
		 */
		int erasePages();

		/**
		 * Validate the overall binary.
		 */
		uint8_t validateApp();

		/**
		 * Store in flash information about the app (start address, checksum, etc.)
		 */
		void storeAppMetadata(uint8_t id, uint16_t checksum, uint16_t size);

	public:
		static MicroApp& getInstance() {
			static MicroApp instance;
			return instance;
		}
		int init();

		/**
		 * Handle incoming events.
		 */
		void handleEvent(event_t & event);

		/**
		 * When fstorage is done, this function will be called (indirectly through app_scheduler).
		 */
		void handleFileStorageEvent(nrf_fstorage_evt_t *evt);
};