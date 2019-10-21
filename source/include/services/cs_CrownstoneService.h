/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Oct 22, 2014
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */
#pragma once

#include <ble/cs_Nordic.h>

#include <ble/cs_Service.h>
#include <ble/cs_Characteristic.h>
#include <events/cs_EventListener.h>

/** General Service for the Crownstone
 *
 * There are several characteristics that fit into the general service description. There is a characteristic
 * that measures the temperature, there are several characteristics that defines the crownstone, namely by
 * name, by type, or by location (room), and there is a characteristic to update its firmware.
 *
 * If meshing is enabled, it is also possible to send a message into the mesh network using a characteristic.
 */
class CrownstoneService: public Service, EventListener {
public:
	/** Constructor for general crownstone service object
	 *
	 * Creates persistent storage (FLASH) object which is used internally to store name and other information that is
	 * set over so-called configuration characteristics. It also initializes all characteristics.
	 */
	 CrownstoneService();

	/** Perform non urgent functionality every main loop.
	 *
	 * Every component has a "tick" function which is for non-urgent things. Urgent matters have to be
	 * resolved immediately in interrupt service handlers. The temperature for example is updated every
	 * tick, because timing is not important for this at all.
	 */
//	void tick();

//	void scheduleNextTick();

	virtual void handleEvent(event_t & event);

protected:
	/** Initialize a CrownstoneService object
	 *
	 * Add all characteristics and initialize them where necessary.
	 */
	void createCharacteristics();

	/** Enable the control characteristic.
 	 */
	void addControlCharacteristic(buffer_ptr_t buffer, uint16_t size, uint16_t charUuid,
			EncryptionAccessLevel minimumAccessLevel = BASIC);

	/** Enable the set configuration characteristic.
	 *
	 * The parameter given with onWrite should actually also already be within the space allocated within the
	 * characteristic.
	 * See <_setConfigurationCharacteristic>.
	 */
	void addConfigurationControlCharacteristic(buffer_ptr_t buffer, uint16_t size,
			EncryptionAccessLevel minimumAccessLevel = ADMIN);

	/** Enable the get configuration characteristic.
	 */
	void addConfigurationReadCharacteristic(buffer_ptr_t buffer, uint16_t size,
			EncryptionAccessLevel minimumAccessLevel = ADMIN);

	inline void addStateControlCharacteristic(buffer_ptr_t buffer, uint16_t size);

	inline void addStateReadCharacteristic(buffer_ptr_t buffer, uint16_t size);

	inline void addFactoryResetCharacteristic();

	void addSessionNonceCharacteristic(buffer_ptr_t buffer, uint16_t size,
			EncryptionAccessLevel minimumAccessLevel = BASIC);

	StreamBuffer<uint8_t>* getStreamBuffer(buffer_ptr_t& buffer, uint16_t& maxLength);

	void removeBuffer();

protected:
	Characteristic<buffer_ptr_t>* _controlCharacteristic;

	/** Set configuration characteristic
	 *
	 * The configuration characteristic reuses the format of the mesh messages. The type are identifiers that are
	 * established:
	 *
	 *  * 0 name
	 *  * 1 device type
	 *  * 2 room
	 *  * 3 floor level
	 *
	 * As you see these are similar to current characteristics and will replace them in the future to save space.
	 * Every characteristic namely occupies a bit of RAM (governed by the SoftDevice, so not under our control).
	 */
	Characteristic<buffer_ptr_t>* _configurationControlCharacteristic;

	/** Get configuration characteristic
	 *
	 * You will have first to select a configuration before you can read from it. You write the identifiers also
	 * described in <_setConfigurationCharacteristic>.
	 *
	 * Then each of these returns a byte array, with e.g. a name, device type, room, etc.
	 */
	Characteristic<buffer_ptr_t>* _configurationReadCharacteristic;

	//! buffer object to read/write configuration characteristics
	StreamBuffer<uint8_t> *_streamBuffer;


	/** Handle write on configuration write characteristic.
	 *
	 * @param[in]  accessLevel    Access level with which the value was written.
	 * @param[in]  value          The (decrypted) data that was written.
	 * @param[in]  length         Length of the data.
	 * @param[out] writeErrCode   Whether or not to write the result.
	 * @return                    Result of handling the data.
	 */
	cs_ret_code_t configOnWrite(const EncryptionAccessLevel accessLevel, const buffer_ptr_t& value, uint16_t length, bool& writeErrCode);

	/** Handle write on state write characteristic.
	 *
	 * @param[in]  accessLevel    Access level with which the value was written.
	 * @param[in]  value          The (decrypted) data that was written.
	 * @param[in]  length         Length of the data.
	 * @param[out] writeErrCode   Whether or not to write the result.
	 * @return                    Result of handling the data.
	 */
	cs_ret_code_t stateOnWrite(const EncryptionAccessLevel accessLevel, const buffer_ptr_t& value, uint16_t length, bool& writeErrCode);

	/** Write the error code to the control characteristic.
	 *
	 * @param[in] type            The command type that was handled.
	 * @param[in] errCode         The result of handling the command.
	 */
	void controlWriteErrorCode(uint8_t type, cs_ret_code_t errCode);


private:
	uint8_t _nonceBuffer[SESSION_NONCE_LENGTH];
	Characteristic<buffer_ptr_t>* _stateControlCharacteristic;
	Characteristic<buffer_ptr_t>* _stateReadCharacteristic;
	Characteristic<buffer_ptr_t>* _sessionNonceCharacteristic;
	Characteristic<uint32_t>*     _factoryResetCharacteristic;
};