/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Jan 2, 2019
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#include "processing/cs_TapToToggle.h"
#include "events/cs_EventDispatcher.h"
#include "drivers/cs_RTC.h"
#include <logging/cs_Logger.h>
#include "processing/cs_CommandHandler.h"
#include "util/cs_Utils.h"
#include "storage/cs_State.h"

#define LOGT2Td LOGnone
#define LOGT2Tv LOGnone

TapToToggle::TapToToggle() {

}

void TapToToggle::init(int8_t defaultRssiThreshold) {
	this->defaultRssiThreshold = defaultRssiThreshold;
	State::getInstance().get(CS_TYPE::CONFIG_TAP_TO_TOGGLE_ENABLED, &enabled, sizeof(enabled));
	TYPIFY(CONFIG_TAP_TO_TOGGLE_RSSI_THRESHOLD_OFFSET) thresholdOffset;
	State::getInstance().get(CS_TYPE::CONFIG_TAP_TO_TOGGLE_RSSI_THRESHOLD_OFFSET, &thresholdOffset, sizeof(thresholdOffset));
	rssiThreshold = defaultRssiThreshold + thresholdOffset;
	LOGd("RSSI threshold = %i", rssiThreshold);
	EventDispatcher::getInstance().addListener(this);
}

void TapToToggle::handleBackgroundAdvertisement(adv_background_parsed_t* adv) {
	LOGT2Td("addr=%x:%x:%x:%x:%x:%x rssi=%i flags=%u", adv->macAddress[0], adv->macAddress[1], adv->macAddress[2], adv->macAddress[3], adv->macAddress[4], adv->macAddress[5], adv->adjustedRssi, adv->flags);
	if (!enabled) {
		return;
	}
	if (adv->adjustedRssi < rssiThreshold) {
		LOGT2Td("rssi too low");
		return;
	}
	if (!CsUtils::isBitSet(adv->flags, BG_ADV_FLAG_TAP_TO_TOGGLE_ENABLED)) {
		LOGT2Td("t2t flag not set");
		return;
	}
	// Use index of entry with matching address, or else with lowest score.
	uint8_t index = -1; // Index to use
	bool foundAddress = false;
	uint8_t lowestScore = 255;
	for (uint8_t i=0; i<T2T_LIST_COUNT; ++i) {
		if (memcmp(list[i].address, adv->macAddress, BLE_GAP_ADDR_LEN) == 0) {
			index = i;
			foundAddress = true;
			break;
		}
		if (list[i].score < lowestScore) {
			lowestScore = list[i].score;
			index = i;
		}
	}
	if (!foundAddress) {
		memcpy(list[index].address, adv->macAddress, BLE_GAP_ADDR_LEN);
		list[index].score = 0;
	}

	// By placing this check here, the score will drop, which will make it more likely to trigger again when phone is kept close.
	if (timeoutCounter != 0) {
		LOGT2Td("wait with t2t");
		return;
	}

	uint8_t prevScore = list[index].score;
	list[index].score += scoreIncrement;
	if (list[index].score > scoreMax) {
		list[index].score = scoreMax;
	}

	LOGT2Td("rssi=%i ind=%u prevScore=%u score=%u", adv->adjustedRssi, index, prevScore, list[index].score);
	if (prevScore <= scoreThreshold && list[index].score > scoreThreshold) {
		LOGi("Tap to toggle triggered");
		timeoutCounter = timeoutTicks;
		event_t event(CS_TYPE::CMD_SWITCH_TOGGLE, nullptr, 0, cmd_source_t(CS_CMD_SOURCE_TAP_TO_TOGLE));
		EventDispatcher::getInstance().dispatch(event);
	}
}

void TapToToggle::tick() {
	for (uint8_t i=0; i<T2T_LIST_COUNT; ++i) {
		if (list[i].score) {
			list[i].score--;
		}
	}
	if (timeoutCounter) {
		timeoutCounter--;
	}
	LOGT2Tv("scores=%u %u %u", list[0].score, list[1].score, list[2].score);
}

void TapToToggle::handleEvent(event_t & event) {
	switch (event.type) {
		case CS_TYPE::EVT_TICK: {
			tick();
			break;
		}
		case CS_TYPE::EVT_ADV_BACKGROUND_PARSED: {
			TYPIFY(EVT_ADV_BACKGROUND_PARSED)* backgroundAdv = (TYPIFY(EVT_ADV_BACKGROUND_PARSED)*)event.data;
			handleBackgroundAdvertisement(backgroundAdv);
			break;
		}
		case CS_TYPE::CONFIG_TAP_TO_TOGGLE_ENABLED: {
			enabled = *((TYPIFY(CONFIG_TAP_TO_TOGGLE_ENABLED)*)event.data);
			break;
		}
		case CS_TYPE::CONFIG_TAP_TO_TOGGLE_RSSI_THRESHOLD_OFFSET: {
			TYPIFY(CONFIG_TAP_TO_TOGGLE_RSSI_THRESHOLD_OFFSET) thresholdOffset = *((TYPIFY(CONFIG_TAP_TO_TOGGLE_RSSI_THRESHOLD_OFFSET)*)event.data);
			rssiThreshold = defaultRssiThreshold + thresholdOffset;
			break;
		}
		default:
			break;
	}
}

