/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Aug 17, 2021
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

struct __attribute__((__packed__)) report_asset_record_t {
	short_asset_id_t assetId;
	/**
	 * Stone id of currently closest stone.
	 *  - Equal to 0 if unknown
	 *  - Equal to _myId if this crownstone believes it's itself
	 *    the closest stone to this asset.
	 */
	stone_id_t winningStoneId;
	/**
	 * Most recent rssi value observed by this crownstone.
	 */
	compressed_rssi_data_t personalRssi;
	/**
	 * rssi between asset and closest stone. only valid if winning_id != 0.
	 */
	compressed_rssi_data_t winningRssi;
};
