/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Mar 10, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include <mesh/cs_MeshCommon.h>
#include <mesh/cs_MeshModelMulticast.h>
#include <mesh/cs_MeshUtil.h>
#include <protocol/mesh/cs_MeshModelPacketHelper.h>
#include <util/cs_BleError.h>

extern "C" {
#include <access.h>
#include <access_config.h>
#include <log.h>
}

static void staticMshHandler(access_model_handle_t handle, const access_message_rx_t * p_message, void * p_args) {
	MeshModelMulticast* meshModel = (MeshModelMulticast*)p_args;
	meshModel->handleMsg(p_message);
}

static const access_opcode_handler_t opcodeHandlers[] = {
		{ACCESS_OPCODE_VENDOR(CS_MESH_MODEL_OPCODE_MSG, CROWNSTONE_COMPANY_ID), staticMshHandler},
};

void MeshModelMulticast::registerMsgHandler(const callback_msg_t& closure) {
	_msgCallback = closure;
}

void MeshModelMulticast::init(uint16_t modelId) {
	assert(_msgCallback != nullptr, "Callback not set");
	uint32_t retVal;
	access_model_add_params_t accessParams;
	accessParams.model_id.company_id = CROWNSTONE_COMPANY_ID;
	accessParams.model_id.model_id = modelId;
	accessParams.element_index = 0;
	accessParams.p_opcode_handlers = opcodeHandlers;
	accessParams.opcode_count = (sizeof(opcodeHandlers) / sizeof((opcodeHandlers)[0]));
	accessParams.p_args = this;
	accessParams.publish_timeout_cb = NULL;
	retVal = access_model_add(&accessParams, &_accessModelHandle);
	APP_ERROR_CHECK(retVal);
	retVal = access_model_subscription_list_alloc(_accessModelHandle);
	APP_ERROR_CHECK(retVal);
}

void MeshModelMulticast::configureSelf(dsm_handle_t appkeyHandle) {
	uint32_t retCode;
	retCode = dsm_address_publish_add(MESH_MODEL_GROUP_ADDRESS, &_groupAddressHandle);
	APP_ERROR_CHECK(retCode);
	retCode = dsm_address_subscription_add_handle(_groupAddressHandle);
	APP_ERROR_CHECK(retCode);

	retCode = access_model_application_bind(_accessModelHandle, appkeyHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_publish_application_set(_accessModelHandle, appkeyHandle);
	APP_ERROR_CHECK(retCode);

	retCode = access_model_publish_address_set(_accessModelHandle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
	retCode = access_model_subscription_add(_accessModelHandle, _groupAddressHandle);
	APP_ERROR_CHECK(retCode);
}

void MeshModelMulticast::handleMsg(const access_message_rx_t * accessMsg) {
	if (accessMsg->meta_data.p_core_metadata->source != NRF_MESH_RX_SOURCE_LOOPBACK) {
		LOGMeshModelVerbose("Handle mesh msg. opcode=%u appkey=%u subnet=%u ttl=%u rssi=%i",
				accessMsg->opcode.opcode,
				accessMsg->meta_data.appkey_handle,
				accessMsg->meta_data.subnet_handle,
				accessMsg->meta_data.ttl,
				MeshUtil::getRssi(accessMsg->meta_data.p_core_metadata)
		);
		MeshUtil::printMeshAddress("  Src: ", &(accessMsg->meta_data.src));
		MeshUtil::printMeshAddress("  Dst: ", &(accessMsg->meta_data.dst));
//		LOGMeshModelVerbose("ownAddress=%u  Data:", _ownAddress);
#if CS_SERIAL_NRF_LOG_ENABLED == 1
		__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Handle mesh msg. opcode=%u appkey=%u subnet=%u ttl=%u rssi=%i\n",
				accessMsg->opcode.opcode,
				accessMsg->meta_data.appkey_handle,
				accessMsg->meta_data.subnet_handle,
				accessMsg->meta_data.ttl,
				MeshUtil::getRssi(accessMsg->meta_data.p_core_metadata)
		);
	}
	else {
		__LOG(LOG_SRC_APP, LOG_LEVEL_INFO, "Handle mesh msg loopback\n");
#endif
	}
//	bool ownMsg = (_ownAddress == accessMsg->meta_data.src.value) && (accessMsg->meta_data.src.type == NRF_MESH_ADDRESS_TYPE_UNICAST);
	bool ownMsg = accessMsg->meta_data.p_core_metadata->source == NRF_MESH_RX_SOURCE_LOOPBACK;
	if (ownMsg) {
		return;
	}
	MeshUtil::cs_mesh_received_msg_t msg =
				MeshUtil::fromAccessMessageRX(*accessMsg);

	cs_result_t result;
	_msgCallback(msg, result);
}

cs_ret_code_t MeshModelMulticast::sendMsg(const uint8_t* data, uint16_t len) {
	access_message_tx_t accessMsg;
	accessMsg.opcode.company_id = CROWNSTONE_COMPANY_ID;
	accessMsg.opcode.opcode = CS_MESH_MODEL_OPCODE_MSG;
	accessMsg.p_buffer = data;
	accessMsg.length = len;
	accessMsg.force_segmented = false;
	accessMsg.transmic_size = NRF_MESH_TRANSMIC_SIZE_SMALL;
	uint32_t status = NRF_SUCCESS;
//	for (int i=0; i<transmissionsOrTimeout; ++i) {
		accessMsg.access_token = nrf_mesh_unique_token_get();
		status = access_model_publish(_accessModelHandle, &accessMsg);
		if (status != NRF_SUCCESS) {
			LOGw("sendMsg failed: %u", status);
//			break;
		}
//	}
	return status;
}

cs_ret_code_t MeshModelMulticast::addToQueue(MeshUtil::cs_mesh_queue_item_t& item) {
	MeshUtil::printQueueItem("Multicast addToQueue", item.metaData);
#if MESH_MODEL_TEST_MSG != 0
	if (item.metaData.type != CS_MESH_MODEL_TYPE_TEST) {
		return ERR_SUCCESS;
	}
#endif
	size16_t msgSize = MeshUtil::getMeshMessageSize(item.msgPayload.len);
	assert(item.msgPayload.data != nullptr || item.msgPayload.len == 0, "Null pointer");
	assert(msgSize != 0, "Empty message");
	assert(msgSize <= MAX_MESH_MSG_NON_SEGMENTED_SIZE, "Message too large");
	assert(item.broadcast == true, "Multicast only");
	assert(item.reliable == false, "Unreliable only");

	// Find an empty spot in the queue (transmissions == 0).
	// Start looking at _queueIndexNext, then reverse iterate over the queue.
	// Then set the new _queueIndexNext at the newly added item, so that it will be sent next.
	// We do the reverse iterate, so that the chance is higher that
	// the old _queueIndexNext will be sent quickly after this newly added item.
	uint8_t index;
	for (int i = _queueIndexNext + _queueSize; i > _queueIndexNext; --i) {
		index = i % _queueSize;
		cs_multicast_queue_item_t* it = &(_queue[index]);
		if (it->metaData.transmissionsOrTimeout == 0) {
			if (!MeshUtil::setMeshMessage((cs_mesh_model_msg_type_t)item.metaData.type, item.msgPayload.data, item.msgPayload.len, it->msg, sizeof(it->msg))) {
				return ERR_WRONG_PAYLOAD_LENGTH;
			}
			memcpy(&(it->metaData), &(item.metaData), sizeof(item.metaData));
			it->msgSize = msgSize;
			LOGMeshModelVerbose("added to ind=%u", index);
			_queueIndexNext = index;

			// TODO: immediately start sending from queue.
			// sendMsgFromQueue can keep up how many msgs have been sent this tick, so it knows how many can still be sent.
			return ERR_SUCCESS;
		}
	}
	LOGw("queue is full");
	return ERR_BUSY;
}

cs_ret_code_t MeshModelMulticast::remFromQueue(cs_mesh_model_msg_type_t type, uint16_t id) {
	cs_ret_code_t retCode = ERR_NOT_FOUND;
	for (int i = 0; i < _queueSize; ++i) {
		if (_queue[i].metaData.id == id && _queue[i].metaData.type == type && _queue[i].metaData.transmissionsOrTimeout != 0) {
			_queue[i].metaData.transmissionsOrTimeout = 0;
			LOGMeshModelVerbose("removed from queue: ind=%u", i);
			retCode = ERR_SUCCESS;
		}
	}
	return retCode;
}

int MeshModelMulticast::getNextItemInQueue(bool priority) {
	int index;
	for (int i = _queueIndexNext; i < _queueIndexNext + _queueSize; i++) {
		index = i % _queueSize;
		if ((!priority || _queue[index].metaData.priority) && _queue[index].metaData.transmissionsOrTimeout > 0) {
			return index;
		}
	}
	return -1;
}

bool MeshModelMulticast::sendMsgFromQueue() {
	int index = getNextItemInQueue(true);
	if (index == -1) {
		index = getNextItemInQueue(false);
	}
	if (index == -1) {
		return false;
	}
	cs_multicast_queue_item_t* item = &(_queue[index]);
//	if (item->type == CS_MESH_MODEL_TYPE_CMD_TIME) {
//		Time time = SystemTime::now();
//		if (time.isValid()) {
//			// Update time in set time command.
//			uint8_t* payload = NULL;
//			size16_t payloadSize = 0;
//			MeshUtil::getPayload(item->msgPtr, item->msgSize, payload, payloadSize);
//			if (payloadSize == sizeof(cs_mesh_model_msg_time_t)) {
//				cs_mesh_model_msg_time_t* timePayload = (cs_mesh_model_msg_time_t*) payload;
//				timePayload->timestamp = time.timestamp();
//			}
//		}
//	}
	sendMsg(item->msg, item->msgSize);
	// TOOD: check return code, maybe retry again later.
	--(item->metaData.transmissionsOrTimeout);
	LOGMeshModelInfo("sent ind=%u transmissions_left=%u type=%u id=%u", index, item->metaData.transmissionsOrTimeout, item->metaData.type, item->metaData.id);

	// Next item will be sent next, so that items are sent interleaved.
	_queueIndexNext = (index + 1) % _queueSize;
	return true;
}

void MeshModelMulticast::processQueue() {
	for (int i=0; i<MESH_MODEL_QUEUE_BURST_COUNT; ++i) {
		if (!sendMsgFromQueue()) {
			break;
		}
	}
}

void MeshModelMulticast::tick(uint32_t tickCount) {
	if (tickCount % (MESH_MODEL_QUEUE_PROCESS_INTERVAL_MS / TICK_INTERVAL_MS) == 0) {
		processQueue();
	}
}
