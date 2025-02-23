/**
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Apr 23, 2015
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <ble/cs_Characteristic.h>
#include <storage/cs_State.h>

#define LOGCharacteristicDebug LOGvv
#define LogLevelCharacteristicDebug SERIAL_VERY_VERBOSE

CharacteristicBase::CharacteristicBase() :
	_name(NULL),
	_handles( { }), _service(0), _status({}), _encryptionBuffer(NULL)
{
}

/**
 * Set the name of the characteristic. This can only be done before initialization.
 */
void CharacteristicBase::setName(const char * const name) {
	if (_status.initialized) BLE_THROW(MSG_BLE_CHAR_INITIALIZED);
	_name = name;

}

/** Set the default attributes of every characteristic
 *
 * There are two settings for the location of the memory of the buffer that is used to communicate with the SoftDevice.
 *
 * + BLE_GATTS_VLOC_STACK
 * + BLE_GATTS_VLOC_USER
 *
 * The former makes the SoftDevice allocate the memory (the quantity of which is defined by the user). The latter
 * leaves it to the user. It is essential that with BLE_GATTS_VLOC_USER the calls to sd_ble_gatts_value_set() are
 * handed persistent pointers to a buffer. If a temporary value is used, this will screw up the data read by the user.
 * The same is true if this buffer is reused across characteristics. If it is meant as data for one characteristic
 * and is read through another, this will resolve to nonsense to the user.
 */
void CharacteristicBase::init(Service* svc) {
	_service = svc;

	CharacteristicInit ci;

	memset(&ci.char_md, 0, sizeof(ci.char_md));
	setupWritePermissions(ci);

	////////////////////////////////////////////////////////////////
	//! attribute metadata for client characteristic configuration //
	////////////////////////////////////////////////////////////////

	memset(&ci.cccd_md, 0, sizeof(ci.cccd_md));
	ci.cccd_md.vloc = BLE_GATTS_VLOC_STACK;
	ci.cccd_md.vlen = 1;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ci.cccd_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ci.cccd_md.write_perm);

	ci.char_md.p_cccd_md = &ci.cccd_md; //! the client characteristic metadata.

	/////////////////////
	//! attribute value //
	/////////////////////

	const ble_uuid_t& uuid = _uuid.getUuid();
	memset(&ci.attr_char_value, 0, sizeof(ci.attr_char_value));

	ci.attr_char_value.p_uuid = &uuid;

	ci.attr_char_value.init_offs = 0;
	ci.attr_char_value.init_len = getGattValueLength();
	ci.attr_char_value.max_len = getGattValueMaxLength();
	ci.attr_char_value.p_value = getGattValuePtr();

	LOGd("%s init with buffer[%i] at %p", _name, getGattValueMaxLength(), getGattValuePtr());

	////////////////////////
	//! attribute metadata //
	////////////////////////

	memset(&ci.attr_md, 0, sizeof(ci.attr_md));
	ci.attr_md.vloc = BLE_GATTS_VLOC_USER;
	ci.attr_md.vlen = 1;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ci.attr_md.read_perm);
	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ci.attr_md.write_perm);
	ci.attr_char_value.p_attr_md = &ci.attr_md;

	//////////////////////////////////////
	//! Characteristic User Description //
	//////////////////////////////////////

	//! these characteristic descriptors are optional, and I gather, not really used by anything.
	//! we fill them in if the user specifies any of the data (eg name).
	ci.char_md.p_char_user_desc = NULL;
	ci.char_md.p_user_desc_md = NULL;

	std::string name = std::string(_name);
	if (!name.empty()) {
		ci.char_md.p_char_user_desc = (uint8_t*) name.c_str(); //! todo utf8 conversion?
		ci.char_md.char_user_desc_size = name.length();
		ci.char_md.char_user_desc_max_size = name.length();

		//! This is the metadata (eg security settings) for the description of this characteristic.
		memset(&ci.user_desc_metadata_md, 0, sizeof(ci.user_desc_metadata_md));

		ci.user_desc_metadata_md.vloc = BLE_GATTS_VLOC_STACK;
		ci.user_desc_metadata_md.vlen = 1;

		BLE_GAP_CONN_SEC_MODE_SET_OPEN(&ci.user_desc_metadata_md.read_perm);

		BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&ci.user_desc_metadata_md.write_perm); //! required

		ci.char_md.p_user_desc_md = &ci.user_desc_metadata_md;
	}

	/////////////////////////
	//! Presentation Format //
	/////////////////////////

	//! presentation format is optional, only fill out if characteristic supports

	ci.char_md.p_char_pf = NULL;
	if (configurePresentationFormat(ci.presentation_format)) {
		ci.presentation_format.unit = 0; //_unit;
		ci.char_md.p_char_pf = &ci.presentation_format;
	}

	//! add all
	uint32_t nrfCode = sd_ble_gatts_characteristic_add(svc->getHandle(), &ci.char_md, &ci.attr_char_value, &_handles);
	switch (nrfCode) {
		case NRF_SUCCESS:
			break;
		case NRF_ERROR_INVALID_ADDR:
			// * @retval ::NRF_ERROR_INVALID_ADDR Invalid pointer supplied.
			// This shouldn't happen: crash.
		case NRF_ERROR_INVALID_PARAM:
			// * @retval ::NRF_ERROR_INVALID_PARAM Invalid parameter(s) supplied, service handle, Vendor Specific UUIDs, lengths, and permissions need to adhere to the constraints.
			// This shouldn't happen: crash.
		case NRF_ERROR_INVALID_STATE:
			// * @retval ::NRF_ERROR_INVALID_STATE Invalid state to perform operation, a service context is required.
			// This shouldn't happen: crash.
		case NRF_ERROR_FORBIDDEN:
			// * @retval ::NRF_ERROR_FORBIDDEN Forbidden value supplied, certain UUIDs are reserved for the stack.
			// This shouldn't happen: crash.
		case NRF_ERROR_NO_MEM:
			// * @retval ::NRF_ERROR_NO_MEM Not enough memory to complete operation.
			// This shouldn't happen: crash.
		case NRF_ERROR_DATA_SIZE:
			// * @retval ::NRF_ERROR_DATA_SIZE Invalid data size(s) supplied, attribute lengths are restricted by @ref BLE_GATTS_ATTR_LENS_MAX.
			// This shouldn't happen: crash.
		default:
			// Crash
			APP_ERROR_HANDLER(nrfCode);
	}

	//! set initial value (default value)
	updateValue();
	_status.initialized = true;
}

/** Setup default write permissions.
 *
 * Structure has the following layout:
 *   // Characteristic Properties.
 *   ble_gatt_char_props_t       char_props;
 *   // Characteristic Extended Properties.
 *   ble_gatt_char_ext_props_t   char_ext_props;
 *   // Pointer to a UTF-8, NULL if the descriptor is not required.
 *   uint8_t                    *p_char_user_desc;
 *   // The maximum size in bytes of the user description descriptor.
 *   uint16_t                    char_user_desc_max_size;
 *   // The size of the user description, must be smaller or equal to char_user_desc_max_size.
 *   uint16_t                    char_user_desc_size;
 *   // Pointer to a presentation format structure or NULL if the descriptor is not required.
 *   ble_gatts_char_pf_t*        p_char_pf;
 *   // Attribute metadata for the User Description descriptor, or NULL for default values.
 *   ble_gatts_attr_md_t*        p_user_desc_md;
 *   // Attribute metadata for the Client Characteristic Configuration Descriptor, or NULL for default values.
 *   ble_gatts_attr_md_t*        p_cccd_md;
 *   // Attribute metadata for the Server Characteristic Configuration Descriptor, or NULL for default values.
 *   ble_gatts_attr_md_t*        p_sccd_md;
 */
void CharacteristicBase::setupWritePermissions(CharacteristicInit& ci) {
	ci.char_md.char_props.read = 1; //! allow read
	ci.char_md.char_props.broadcast = 0; //! don't allow broadcast
	ci.char_md.char_props.write = _status.writable ? 1 : 0;
	ci.char_md.char_props.notify = _status.notifies ? 1 : 0;
	ci.char_md.char_props.indicate = _status.indicates ? 1 : 0;

	ci.attr_md.write_perm = _writeperm;
}

/** Update characteristic. This is also required when switching to/from encryption.
 */
uint32_t CharacteristicBase::updateValue(ConnectionEncryptionType encryptionType) {

//	LOGi("[%s] update Value", _name);

	//! get the data length of the value (unencrypted)
	uint16_t valueLength = getValueLength();
	/* get the address where the value should be stored so that the
	 * gatt server can access it
	 */
	uint8_t* valueGattAddress = getGattValuePtr();

	if (_status.aesEncrypted && _minAccessLevel < ENCRYPTION_DISABLED) {
		// GATT is public facing, getValue is internal
		// getValuePtr is not padded, it's the size of an int, or string or whatever is required.
		// the valueGattAddress can be used as buffer for encryption
		_log(LogLevelCharacteristicDebug, false, "data: ");
		_logArray(LogLevelCharacteristicDebug, true, getValuePtr(), valueLength);

		// we calculate what size buffer we need
		cs_ret_code_t retVal = ERR_SUCCESS;
		uint16_t encryptionBufferLength = ConnectionEncryption::getEncryptedBufferSize(valueLength, encryptionType);
		if (encryptionBufferLength > getGattValueMaxLength()) {
			retVal = ERR_BUFFER_TOO_SMALL;
		}
		else {
			retVal = ConnectionEncryption::getInstance().encrypt(
					cs_data_t(getValuePtr(), valueLength),
					cs_data_t(valueGattAddress, encryptionBufferLength),
					_minAccessLevel,
					encryptionType
			);
			_log(LogLevelCharacteristicDebug, false, "encrypted: ");
			_logArray(LogLevelCharacteristicDebug, true, valueGattAddress, encryptionBufferLength);
		}
		if (!SUCCESS(retVal)) {
			// clear the partially encrypted buffer.
			memset(valueGattAddress, 0x00, encryptionBufferLength);

			// make sure nothing will be read
			setGattValueLength(0);

			// disconnect from the device.
			Stack::getInstance().disconnect();
			LOGe("error encrypting data.");
			return retVal;
		}

		// on success, set the readable buffer length to the encryption package.
		setGattValueLength(encryptionBufferLength);
	}
	else {
		//! set the data length of the gatt value (when not using encryption)
		setGattValueLength(valueLength);
	}


	uint16_t gattValueLength = getGattValueLength();
	LOGCharacteristicDebug("gattValueLength=%u gattValueAddress=%p, gattValueMaxSize=%u", gattValueLength, valueGattAddress, getGattValueMaxLength());

	ble_gatts_value_t gatts_value;
	gatts_value.len = gattValueLength;
	gatts_value.offset = 0;
	gatts_value.p_value = valueGattAddress;
	
	uint32_t nrfCode = sd_ble_gatts_value_set(
			_service->getStack()->getConnectionHandle(),
			_handles.value_handle,
			&gatts_value
	);
	switch (nrfCode) {
		case NRF_SUCCESS:
			break;
		case BLE_ERROR_INVALID_CONN_HANDLE:
			// * @retval ::BLE_ERROR_INVALID_CONN_HANDLE Invalid connection handle supplied on a system attribute.
			// This shouldn't happen, as the connection handle is only set in the main thread.
			LOGe("Invalid handle");
			break;
		case NRF_ERROR_INVALID_ADDR:
			// * @retval ::NRF_ERROR_INVALID_ADDR Invalid pointer supplied.
			// This shouldn't happen: crash.
		case NRF_ERROR_INVALID_PARAM:
			// * @retval ::NRF_ERROR_INVALID_PARAM Invalid parameter(s) supplied.
			// This shouldn't happen: crash.
		case NRF_ERROR_NOT_FOUND:
			// * @retval ::NRF_ERROR_NOT_FOUND Attribute not found.
			// This shouldn't happen: crash.
		case NRF_ERROR_FORBIDDEN:
			// * @retval ::NRF_ERROR_FORBIDDEN Forbidden handle supplied, certain attributes are not modifiable by the application.
			// This shouldn't happen: crash.
		case NRF_ERROR_DATA_SIZE:
			// * @retval ::NRF_ERROR_DATA_SIZE Invalid data size(s) supplied, attribute lengths are restricted by @ref BLE_GATTS_ATTR_LENS_MAX.
			// This shouldn't happen: crash.
		default:
			// Crash
			APP_ERROR_HANDLER(nrfCode);
	}

	// Stop here if we are not in notifying state
	if ((!_status.notifies) || (!_service->getStack()->isConnectedPeripheral()) || !_status.notifyingEnabled) {
		return ERR_SUCCESS;
	}
	else {
		return notify();
	}
}


uint32_t CharacteristicBase::notify() {

	if (!_status.notifies || !_service->getStack()->isConnectedPeripheral() || !_status.notifyingEnabled) {
		return NRF_ERROR_INVALID_STATE;
	}

	uint32_t nrfCode;

	// Get the data length of the value (encrypted)
	uint16_t valueLength = getGattValueLength();

	ble_gatts_hvx_params_t hvx_params;

	hvx_params.handle = _handles.value_handle;
	hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
	hvx_params.offset = 0;
	hvx_params.p_len = &valueLength;
	hvx_params.p_data = NULL;

	nrfCode = sd_ble_gatts_hvx(_service->getStack()->getConnectionHandle(), &hvx_params);

	switch (nrfCode) {
		case NRF_SUCCESS:
			break;
		case NRF_ERROR_RESOURCES:
			// * @retval ::NRF_ERROR_RESOURCES Too many notifications queued.
			// *                               Wait for a @ref BLE_GATTS_EVT_HVN_TX_COMPLETE event and retry.
			// Mark that there is a pending notification, we can retry later.
			onNotifyTxError();
			break;
		case NRF_ERROR_TIMEOUT:
			// * @retval ::NRF_ERROR_TIMEOUT There has been a GATT procedure timeout. No new GATT procedure can be performed without reestablishing the connection.
			// It can happen there was a timeout in the meantime.
		case NRF_ERROR_INVALID_STATE: {
			// * @retval ::NRF_ERROR_INVALID_STATE One or more of the following is true:
			// *                                   - Invalid Connection State
			// *                                   - Notifications and/or indications not enabled in the CCCD
			// *                                   - An ATT_MTU exchange is ongoing
			// It can happen that the phone disconnected or disabled notification in the meantime.
			LOGw("nrfCode=%u (0x%X)", nrfCode, nrfCode);

			// Reset the notification state, we can't retry later.
			_status.notificationPending = false;
			break;
		}
		case BLE_ERROR_GATTS_SYS_ATTR_MISSING: {
			// * @retval ::BLE_ERROR_GATTS_SYS_ATTR_MISSING System attributes missing, use @ref sd_ble_gatts_sys_attr_set to set them to a known value.
			// TODO: Currently excluded from APP_ERROR_CHECK, seems to originate from MESH code
			LOGe("nrfCode=%u (0x%X)", nrfCode, nrfCode);
			break;
		}
		default:
			// Other return codes, see Characteristic<buffer_ptr_t>::notify().
			// Crash
			APP_ERROR_HANDLER(nrfCode);
	}

	return nrfCode;
}


#define MAX_NOTIFICATION_LEN 19

struct __attribute__((__packed__)) notification_t {
	uint8_t partNr;
	uint8_t data[MAX_NOTIFICATION_LEN];
};

uint32_t Characteristic<buffer_ptr_t>::notify() {

	if (!CharacteristicBase::_status.notifies || !_service->getStack()->isConnectedPeripheral() || !_status.notifyingEnabled) {
		return NRF_ERROR_INVALID_STATE;
	}

	uint32_t nrfCode = NRF_SUCCESS;

	// get the data length of the value (encrypted)
	uint16_t valueLength = getGattValueLength();
	uint8_t* valueGattAddress = getGattValuePtr();

	ble_gatts_hvx_params_t hvx_params;

	uint16_t offset;
	if (_status.notificationPending) {
		offset = _notificationPendingOffset;
	}
	else {
		offset = 0;
	}

	while (offset < valueLength) {
		uint16_t dataLen = MIN(valueLength - offset, MAX_NOTIFICATION_LEN);

		notification_t notification = {};

		// TODO: [Alex 22.08] verify if the oldVal is required. I do not think we use this.
		uint8_t oldVal[sizeof(notification_t)];

		if (valueLength - offset > MAX_NOTIFICATION_LEN) {
			notification.partNr = offset / MAX_NOTIFICATION_LEN;
		}
		else {
			notification.partNr = CS_CHARACTERISTIC_NOTIFICATION_PART_LAST;
		}

//		LOGi("dataLen: %d ", dataLen);
//		CsUtils::printArray(valueGattAddress, valueLength);

		memcpy(&notification.data, valueGattAddress + offset, dataLen);

		uint16_t packageLen = dataLen + sizeof(notification.partNr);
		uint8_t* p_data = (uint8_t*)&notification;

//		LOGi("address: %p", valueGattAddress + offset);
//		LOGi("offset: %d", offset);
//		CsUtils::printArray((uint8_t*)valueGattAddress + offset, dataLen);
//		CsUtils::printArray((uint8_t*)&notification, packageLen);

		memcpy(oldVal, valueGattAddress + offset, packageLen);

		hvx_params.handle = _handles.value_handle;
		hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
		hvx_params.offset = 0;
		hvx_params.p_len = &packageLen;
		hvx_params.p_data = p_data;

		nrfCode = sd_ble_gatts_hvx(_service->getStack()->getConnectionHandle(), &hvx_params);
		switch (nrfCode) {
			case NRF_SUCCESS: {
				memcpy(valueGattAddress + offset, oldVal, packageLen);
				offset += dataLen;
				break;
			}

			case NRF_ERROR_RESOURCES: {
				// * @retval ::NRF_ERROR_RESOURCES Too many notifications queued.
				// *                               Wait for a @ref BLE_GATTS_EVT_HVN_TX_COMPLETE event and retry.
				// Mark that there is a pending notification, we can continue later.
				onNotifyTxError();
				_notificationPendingOffset = offset;
				return nrfCode;
			}

			case NRF_ERROR_TIMEOUT:
				// * @retval ::NRF_ERROR_TIMEOUT There has been a GATT procedure timeout. No new GATT procedure can be performed without reestablishing the connection.
				// It can happen there was a timeout in the meantime.
			case NRF_ERROR_INVALID_STATE: {
				// * @retval ::NRF_ERROR_INVALID_STATE One or more of the following is true:
				// *                                   - Invalid Connection State
				// *                                   - Notifications and/or indications not enabled in the CCCD
				// *                                   - An ATT_MTU exchange is ongoing
				// It can happen that the phone disconnected or disabled notification in the meantime.
				LOGw("nrfCode=%u (0x%X)", nrfCode, nrfCode);

				// Reset the notification state, we can't continue later.
				_notificationPendingOffset = 0;
				_status.notificationPending = false;
				return nrfCode;
			}

			case BLE_ERROR_GATTS_SYS_ATTR_MISSING: {
				// * @retval ::BLE_ERROR_GATTS_SYS_ATTR_MISSING System attributes missing, use @ref sd_ble_gatts_sys_attr_set to set them to a known value.
				// Anne: do not complain for now... (meshing)
				LOGe("nrfCode=%u (0x%X)", nrfCode, nrfCode);
				break;
			}

			case BLE_ERROR_INVALID_CONN_HANDLE: {
				// * @retval ::BLE_ERROR_INVALID_CONN_HANDLE Invalid Connection Handle.
				// This shouldn't happen, as the connection handle is only set in the main thread.
				LOGe("Invalid handle");
				break;
			}

			case NRF_ERROR_BUSY:
				// * @retval ::NRF_ERROR_BUSY For @ref BLE_GATT_HVX_INDICATION Procedure already in progress. Wait for a @ref BLE_GATTS_EVT_HVC event and retry.
				// This shouldn't happen, as we don't use INDICATION, but NOTIFICATION.
			case NRF_ERROR_INVALID_ADDR:
				// * @retval ::NRF_ERROR_INVALID_ADDR Invalid pointer supplied.
				// This shouldn't happen: crash.
			case NRF_ERROR_INVALID_PARAM:
				// * @retval ::NRF_ERROR_INVALID_PARAM Invalid parameter(s) supplied.
				// This shouldn't happen: crash.
			case BLE_ERROR_INVALID_ATTR_HANDLE:
				// * @retval ::BLE_ERROR_INVALID_ATTR_HANDLE Invalid attribute handle(s) supplied. Only attributes added directly by the application are available to notify and indicate.
				// This shouldn't happen: crash.
			case BLE_ERROR_GATTS_INVALID_ATTR_TYPE:
				// * @retval ::BLE_ERROR_GATTS_INVALID_ATTR_TYPE Invalid attribute type(s) supplied, only characteristic values may be notified and indicated.
				// This shouldn't happen: crash.
			case NRF_ERROR_NOT_FOUND:
				// * @retval ::NRF_ERROR_NOT_FOUND Attribute not found.
				// This shouldn't happen: crash.
			case NRF_ERROR_FORBIDDEN:
				// * @retval ::NRF_ERROR_FORBIDDEN The connection's current security level is lower than the one required by the write permissions of the CCCD associated with this characteristic.
				// This shouldn't happen: crash.
			case NRF_ERROR_DATA_SIZE:
				// * @retval ::NRF_ERROR_DATA_SIZE Invalid data size(s) supplied.
				// This shouldn't happen: crash.
			default:
				// Crash
				APP_ERROR_HANDLER(nrfCode);
		}
	}
	return nrfCode;
}

/** When a previous transmission is successful send the next.
 */
void CharacteristicBase::onTxComplete(__attribute__((unused))const ble_common_evt_t * p_ble_evt) {
	// if we have a notification pending, try to send it
	if (_status.notificationPending) {
		// this-> is necessary so that the call of notify depends on the template
		// parameter T
		uint32_t err_code = this->notify();
		if (err_code == NRF_SUCCESS) {
//				LOGi("ontx success");
			_status.notificationPending = false;
		}
	}
}

void Characteristic<buffer_ptr_t>::onTxComplete(__attribute__((unused))const ble_common_evt_t * p_ble_evt) {
	// if we have a notification pending, try to send it
	if (_status.notificationPending) {
		// this-> is necessary so that the call of notify depends on the template
		// parameter T
		uint32_t err_code = this->notify();
		if (err_code == NRF_SUCCESS) {
//				LOGi("ontx success");
			_status.notificationPending = false;
			_notificationPendingOffset = 0;
		}
	}
}

