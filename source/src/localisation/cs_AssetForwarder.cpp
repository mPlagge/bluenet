/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: May 12, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */


#include <localisation/cs_AssetForwarder.h>
#include <mesh/cs_MeshMsgEvent.h>

#include <protocol/cs_RssiAndChannel.h>
#include <protocol/cs_Packets.h>
#include <uart/cs_UartHandler.h>
#include <logging/cs_Logger.h>
#include <storage/cs_State.h>

#define LOGAssetForwarderDebug LOGvv

void printAsset(const cs_mesh_model_msg_asset_report_mac_t& assetMsg) {
	LOGAssetForwarderDebug("mesh_model_msg_asset: ch%u @ %i dB",
			getChannel(assetMsg.rssiData),
			getRssi(assetMsg.rssiData));
}


cs_ret_code_t AssetForwarder::init() {
	State::getInstance().get(CS_TYPE::CONFIG_CROWNSTONE_ID, &_myStoneId, sizeof(_myStoneId));
	listen();
	return ERR_SUCCESS;
}

// ------------- outbox management -------------

void AssetForwarder::flush() {
	for(auto outMsg : outbox) {
		dispatchOutboxMessage(outMsg);
	}

	clearOutbox();
}

void AssetForwarder::clearOutbox() {
	memset(&outbox, 0x00, sizeof(outbox));
}

AssetForwarder::outbox_msg_t* AssetForwarder::getEmptyOutboxSlot() {
	for(auto& msg : outbox) {
		if (!msg.isValid()) {
			return &msg;
		}
	}
	return nullptr;
}

void AssetForwarder::setThrottleCountdownBumpTicks(uint8_t ticks) {
	_throttleCountdownBumpTicks = ticks;
}

// ------------- message management -------------

void AssetForwarder::sendAssetMacToMesh(asset_record_t* record, const scanned_device_t& asset) {
	LOGAssetForwarderDebug("Forward mac-over-mesh ch%u, %d dB", asset.channel, asset.rssi);

	outbox_msg_t* outMsg = getEmptyOutboxSlot();

	if(outMsg == nullptr) {
		return;
	}

	outMsg->macMsg.rssiData = rssi_and_channel_t(asset.rssi, asset.channel);
	outMsg->macMsg.mac.copy(asset.address);

	//	return MIN_THROTTLED_ADVERTISEMENT_PERIOD_MS;
}


void AssetForwarder::sendAssetIdToMesh(asset_record_t* record, const scanned_device_t& asset, const asset_id_t& assetId, uint8_t filterBitmask) {
	LOGAssetForwarderDebug("Forward sid-over-mesh ch%u, %d dB", asset.channel, asset.rssi);
	// TODO: merge messages that differ only in filterbitmask

	outbox_msg_t* outMsg = getEmptyOutboxSlot();

	if(outMsg == nullptr) {
		return;
	}

	outMsg->rec = record;
	outMsg->msgType = CS_MESH_MODEL_TYPE_ASSET_INFO_ID;

	outMsg->idMsg.id = assetId;
	outMsg->idMsg.rssi= asset.rssi;
	outMsg->idMsg.channel = compressChannel(asset.channel);
	outMsg->idMsg.filterBitmask = filterBitmask;
}


void AssetForwarder::dispatchOutboxMessage(outbox_msg_t outMsg) {
	if(!outMsg.isValid()) {
		return;
	}

	if (outMsg.rec != nullptr) {
		outMsg.rec->addThrottlingCountdown(_throttleCountdownBumpTicks);
	}

	// forward message over uart (e.g. hub dongle directly receives asset advertisement)
	switch (outMsg.msgType) {
		case CS_MESH_MODEL_TYPE_ASSET_INFO_MAC: {
			forwardAssetToUart(outMsg.macMsg, _myStoneId);
			break;
		}
		case CS_MESH_MODEL_TYPE_ASSET_INFO_ID: {
			forwardAssetToUart(outMsg.idMsg, _myStoneId);
			break;
		}
		default: {
			// outMsg invalid. shouldn't occur as it is already checked.
			return;
		}
	}

	cs_mesh_msg_t msgWrapper;
	msgWrapper.type        = outMsg.msgType;
	msgWrapper.payload     = outMsg.rawMsg;
	msgWrapper.size        = sizeof(outMsg.rawMsg);
	msgWrapper.reliability = CS_MESH_RELIABILITY_LOW;
	msgWrapper.urgency     = CS_MESH_URGENCY_LOW;

	event_t meshMsgEvt(CS_TYPE::CMD_SEND_MESH_MSG, &msgWrapper, sizeof(msgWrapper));
	meshMsgEvt.dispatch();
}


// ------------- private stuff -------------

void AssetForwarder::handleEvent(event_t & event) {
	switch (event.type) {
		case CS_TYPE::EVT_RECV_MESH_MSG: {
			auto meshMsg = CS_TYPE_CAST(EVT_RECV_MESH_MSG, event.data);

			switch(meshMsg->type) {
				case CS_MESH_MODEL_TYPE_ASSET_INFO_MAC: {
					forwardAssetToUart(meshMsg->getPacket<CS_MESH_MODEL_TYPE_ASSET_INFO_MAC>(), meshMsg->srcAddress);
					event.result.returnCode = ERR_SUCCESS;
					break;
				}
				case CS_MESH_MODEL_TYPE_ASSET_INFO_ID: {
					forwardAssetToUart(meshMsg->getPacket<CS_MESH_MODEL_TYPE_ASSET_INFO_ID>(), meshMsg->srcAddress);
					event.result.returnCode = ERR_SUCCESS;
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		default:
			break;
	}
}

void AssetForwarder::forwardAssetToUart(const cs_mesh_model_msg_asset_report_mac_t& assetMsg, stone_id_t seenByStoneId) {
	printAsset(assetMsg);

	auto uartAssetMsg = asset_report_uart_mac_t {
			.address = assetMsg.mac,
			.stoneId = seenByStoneId,
			.rssi    = assetMsg.rssiData.getRssi(),
			.channel = assetMsg.rssiData.getChannel(),
	};

	UartHandler::getInstance().writeMsg(
			UartOpcodeTx::UART_OPCODE_TX_ASSET_INFO_MAC,
			reinterpret_cast<uint8_t*>(&uartAssetMsg),
			sizeof(uartAssetMsg));
}

void AssetForwarder::forwardAssetToUart(const cs_mesh_model_msg_asset_report_id_t& assetMsg, stone_id_t seenByStoneId) {
	LOGAssetForwarderDebug("forwarding asset id msg to uart");

	auto uartAssetMsg = asset_report_uart_id_t{
			.assetId         = assetMsg.id,
			.stoneId         = seenByStoneId,
			.filterBitmask   = assetMsg.filterBitmask,
			.rssi            = assetMsg.rssi,
			.channel         = decompressChannel(assetMsg.channel),
	};

	UartHandler::getInstance().writeMsg(
			UartOpcodeTx::UART_OPCODE_TX_ASSET_INFO_SID,
			reinterpret_cast<uint8_t*>(&uartAssetMsg),
			sizeof(uartAssetMsg));
}
