/***********************************************************************************************//**
 * \file   main.c
 * \brief  Silicon Labs BT Mesh Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko BT Mesh C application.
 * The application starts unprovisioned Beaconing after boot
 ***************************************************************************************************
 * <b> (C) Copyright 2017 Silicon Labs, http://www.silabs.com</b>
 ***************************************************************************************************
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 **************************************************************************************************/

/* Board headers */
#include "init_mcu.h"
#include "init_board.h"
#include "init_app.h"
#include "ble-configuration.h"
#include "board_features.h"

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"
#include <gecko_configuration.h>
#include <mesh_sizes.h>

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#include <em_gpio.h>

/* Device initialization header */
#include "hal-config.h"

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

#include "log.h"
/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

// bluetooth stack heap
                    #define MAX_CONNECTIONS 2
// One for bgstack; one for general mesh operation;
// N for mesh GATT service advertisements
#define MAX_ADVERTISERS (2 + 4)

uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS) + BTMESH_HEAP_SIZE + 1760];

// bluetooth stack configuration
extern const struct bg_gattdb_def bg_gattdb_data;

// Flag for indicating DFU Reset must be performed
uint8_t boot_to_dfu = 0;

const gecko_configuration_t config =
{
  .bluetooth.max_connections = MAX_CONNECTIONS,
  .bluetooth.max_advertisers = MAX_ADVERTISERS,
  .bluetooth.heap = bluetooth_stack_heap,
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap) - BTMESH_HEAP_SIZE,
  .bluetooth.sleep_clock_accuracy = 100,
  .gattdb = &bg_gattdb_data,
  .btmesh_heap_size = BTMESH_HEAP_SIZE,
#if (HAL_PA_ENABLE) && defined(FEATURE_PA_HIGH_POWER)
  .pa.config_enable = 1, // Enable high power PA
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT, // Configure PA input to VBAT
#endif // (HAL_PA_ENABLE) && defined(FEATURE_PA_HIGH_POWER)
};

static void handle_gecko_event(uint32_t evt_id, struct gecko_cmd_packet *evt);
void mesh_native_bgapi_init(void);
bool mesh_bgapi_listener(struct gecko_cmd_packet *evt);

const uint8_t static_oob_data[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
const uint8_t auth[20] = {
          // value bytes here
         0x42, 0xd2, 0xe1, 0x92, 0x03, 0x07, 0x1a, 0x38, 0xde, 0x9c, 0x28, 0x38, 0x19, 0xa1, 0x9a, 0xd4, 0xcc, 0x19, 0xc6, 0x80};
int main()
{
  // Initialize device
  initMcu();
  // Initialize board
  initBoard();
  // Initialize application
  initApp();

  gecko_stack_init(&config);
  gecko_bgapi_class_dfu_init();
  gecko_bgapi_class_system_init();
  gecko_bgapi_class_le_gap_init();
  gecko_bgapi_class_le_connection_init();
  gecko_bgapi_class_gatt_init();
  gecko_bgapi_class_gatt_server_init();
  gecko_bgapi_class_endpoint_init();
  gecko_bgapi_class_hardware_init();
  gecko_bgapi_class_flash_init();
  gecko_bgapi_class_test_init();
  gecko_bgapi_class_sm_init();
  mesh_native_bgapi_init();
  gecko_initCoexHAL();

  INIT_LOG();
    LOGI(RTT_CTRL_CLEAR"--------- Compiled - %s - %s ---------\r\n", (uint32_t)__DATE__, (uint32_t)__TIME__);

  while (1) {
    struct gecko_cmd_packet *evt = gecko_wait_event();
    bool pass = mesh_bgapi_listener(evt);
    if (pass) {
      handle_gecko_event(BGLIB_MSG_ID(evt->header), evt);
    }
  }
}

static void handle_gecko_event(uint32_t evt_id, struct gecko_cmd_packet *evt)
{
  switch (evt_id) {
    case gecko_evt_system_boot_id:
    	LOGW("Factory reset started.\r\n");
    	gecko_cmd_flash_ps_erase_all();
    	LOGW("Factory reset done.\r\n");

    	if(gecko_cmd_flash_ps_save(0x1700, 20, auth)->result)
    		LOGE("Write Auth Error.\r\n");
    	else
    		LOGD("Write Auth Success.\r\n");

    	if(gecko_cmd_mesh_node_init_oob(0, 2, 0, 0, 0, 0, 0)->result)
    		LOGE("Init OOB Error.\r\n");
    	else
    		LOGD("Init OOB Success.\r\n");
      break;

//    case gecko_evt_mesh_node_static_oob_request_id:
//    	if(gecko_cmd_mesh_node_static_oob_request_rsp(16, static_oob_data)->result)
//    		LOGE("Rsp OOB Data Error.\r\n");
//    	 else
//    	    LOGD("Rsp OOB Data Success.\r\n");
//    	break;

    case gecko_evt_mesh_node_initialized_id:
    	LOGD("Started Unprov Beancons.\r\n");
      // The Node is now initialized, start unprovisioned Beaconing using PB-Adv Bearer
      gecko_cmd_mesh_node_start_unprov_beaconing(0x1);
      break;

    case gecko_evt_mesh_node_provisioned_id:
    	LOGI("Provisioned.\r\n");
    	break;
    case gecko_evt_mesh_node_provisioning_failed_id:
    	LOGE("Provisioning failed. Reason = 0x%04X\r\n", evt->data.evt_mesh_node_provisioning_failed.result);
    	break;
    case gecko_evt_mesh_node_provisioning_started_id:
    	LOGI("Provisioning started.\r\n");
    	break;

    default:
      break;
  }
}
