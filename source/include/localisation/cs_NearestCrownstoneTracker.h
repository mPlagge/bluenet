/*
 * Author: Crownstone Team
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: Nov 17, 2020
 * License: LGPLv3+, Apache License 2.0, and/or MIT (triple-licensed)
 */

#pragma once

#include <events/cs_EventListener.h>
#include <localisation/cs_Nearestnearestwitnessreport.h>
#include <localisation/cs_TrackableEvent.h>
#include <protocol/cs_Typedefs.h>
#include <protocol/mesh/cs_MeshModelPackets.h>

#include <localisation/cs_AssetRecord.h>
#include <localisation/cs_AssetHandler.h>

/**
 * This class implements the in-mesh computation of which crownstone
 * is nearest to a 'trackable' device.
 *
 * When a trackable device broadcasts an advertisement, and the TrackableParser
 * consequently dispatches a TrackableEvent, it is this class that performs the necessary
 * communication and computation to decide whether or not this crownstone is closest to
 * the trackable device.
 *
 * It is possible to enable this component on a subset of the crownstones in a mesh
 * if necessary (for optimization purposes perhaps). When doing that, the resulting nearest
 * crownstone will be an element of this subset.
 */
class NearestCrownstoneTracker: public EventListener {
public:
	/**
	 * Caches CONFIG_CROWNSTONE_ID, and resets the _assetRecords cache.
	 */
	void init();

private:
	static constexpr auto MAX_REPORTS = 10u;

	stone_id_t _myId; // cached for efficiency

	/**
	 * Relevant data for this algorithm per asset_id.
	 * Possible strategies to reduce memory:
	 *  - when full, remove worst personal_rssi.
	 *  - only store if observed the asset personally.
	 *  - ...
	 *
	 *  The array is 'front loaded'. entries with index < _assetRecordCount
	 *  are valid, other entries are not.
	 */
	report_asset_record_t _assetRecords[MAX_REPORTS];

	/**
	 * Current number of valid records in the _assetRecords array.
	 */
	uint8_t _assetRecordCount = 0;

	// -------------------------------------------
	// ------------- Incoming events -------------
	// -------------------------------------------

	/**
	 * Called on EVT_RECV_MESH_MSG events.
	 * Extract report from other crownstones that come in from the mesh
	 * and call onReceiveAssetReport.
	 */
	void handleMeshMsgEvent(event_t& evt);

	/**
	 * Called on EVT_ASSET_ACCEPTED events.
	 * Transforms an accepted asset event into a report and handles it
	 * by calling onReceivedAssetAdvertisement.
	 */
	void handleAssetAcceptedEvent(event_t& evt);

	/**
	 * Heart of the algorithm. See implementation for exact behaviour.
	 *
	 * This method implements what a crownstone needs to communicate when
	 * it directly receives an advertisement from a trackable device.
	 *
	 * Updates personal report, possibly updates winning report
	 * and possibly broadcasts a message to inform other devices
	 * in the mesh of relevant changes.
	 */
	void onReceiveAssetAdvertisement(report_asset_id_t& trackedEvent);

	/**
	 * Heart of the algorithm. See implementation for exact behaviour.
	 *
	 * This method implements what a crownstone needs to communicate when
	 * it receives a message from another crownstone containing information
	 * about a trackable device.
	 *
	 * Possibly updates winning report and possibly broadcasts
	 * a message to inform other devices in the mesh of relevant changes.
	 * E.g. when the updated winning report now loses from this devices
	 * personal report.
	 */
	void onReceiveAssetReport(report_asset_id_t& report, stone_id_t reporter);

	// -------------------------------------------
	// ------------- Outgoing events -------------
	// -------------------------------------------

	/**
	 * Sends a mesh broadcast for the given report.
	 * (stone id of reporter is contained in bluetooth metadata)
	 */
	void broadcastReport(report_asset_id_t& report);

	/**
	 * makes a report from the records personalRssi field
	 * and broadcast it over the mesh.
	 */
	void broadcastPersonalReport(report_asset_record_t& record);

	/**
	 * Sends a message over UART containing the winner, its rssi
	 * and the asset id.
	 */
	void sendUartUpdate(report_asset_record_t& record);

	/**
	 * Report to uart host that this report is timing out.
	 */
	void sendUartTimeout(const short_asset_id_t& record);

	/**
	 * Currently dispatches an event for CMD_SWITCH_[ON,OFF] depending on
	 * whether this crownstone is closest (ON) or not the closest.
	 *
	 * Also outputs the current winner to UART for demo purposes.
	 */
	void onWinnerChanged(bool winnerIsThisCrownstone);

	// ---------------------------------
	// ------------- Utils -------------
	// ---------------------------------


	/**
	 * saves the rssi as personal
	 */
	void savePersonalReport(report_asset_record_t& rec, compressed_rssi_data_t personalRssi);

	/**
	 * saves the report in the _winning list.
	 */
	void saveWinningReport(report_asset_record_t& rec, compressed_rssi_data_t winningRssi, stone_id_t winningId);

	/**
	 * returns a pointer to the found record, possibly empty.
	 * returns nullptr if not found and creating a new one was
	 * impossible.
	 */
	report_asset_record_t* getOrCreateRecord(short_asset_id_t& id);

	/**
	 * Assumes my_id is set to the stone id of this crownstone.
	 * Sets the reporter id of the personal report to my_id.
	 * Sets the reporter id of the winning report to 0.
	 * Sets the rssi value of both these reports to -127.
	 */
	void resetReports();


	void logRecord(report_asset_record_t& record);

	/**
	 * Log a warning.
	 */
	void onAssetListFull(report_asset_id_t& report);

public:
	/**
	 * Handlers for:
	 * EVT_MESH_NEAREST_WITNESS_REPORT
	 * EVT_ASSET_ACCEPTED
	 * EVT_FILTERS_UPDATED
	 */
	void handleEvent(event_t &evt);

};

