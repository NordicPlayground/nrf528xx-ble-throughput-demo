/* Copyright (c) 2016 Nordic Semiconductor. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the license.txt file.
 */

/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#include "sdk_common.h"
#include "nrf_error.h"
#include "ble_err.h"
#include "ble_srv_common.h"
#include "app_error.h"
#include "amt.h"

#define NRF_LOG_MODULE_NAME "AMTS"
#include "nrf_log.h"


#define OPCODE_LENGTH 1     /**< Length of opcode inside a notification. */
#define HANDLE_LENGTH 2     /**< Length of handle inside a notification. */


static void char_notification_send(nrf_ble_amts_t * p_ctx);


/**@brief Function for handling the Connect event.
 *
 * @param     p_ctx       Pointer to the AMTS structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_connect(nrf_ble_amts_t * p_ctx, ble_evt_t * p_ble_evt)
{
    p_ctx->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param     p_ctx       Pointer to the AMTS structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_disconnect(nrf_ble_amts_t * p_ctx, ble_evt_t * p_ble_evt)
{
    p_ctx->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the TX_COMPLETE event.
 *
 * @param     p_ctx       Pointer to the AMTS structure.
 */
static void on_tx_complete(nrf_ble_amts_t * p_ctx)
{
    if (p_ctx->busy)
    {
        p_ctx->busy = false;
        char_notification_send(p_ctx);
    }
}


/**@brief Function for handling the Write event.
 *
 * @param     p_ctx       Pointer to the AMTS structure.
 * @param[in] p_ble_evt   Event received from the BLE stack.
 */
static void on_write(nrf_ble_amts_t * p_ctx, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if ((p_evt_write->handle == p_ctx->amts_char_handles.cccd_handle) &&
        (p_evt_write->len == 2))
    {
        // CCCD written, call the application event handler.
        nrf_ble_amts_evt_t evt;

        if (ble_srv_is_notification_enabled(p_evt_write->data))
        {
            evt.evt_type = SERVICE_EVT_NOTIF_ENABLED;
        }
        else
        {
            evt.evt_type = SERVICE_EVT_NOTIF_DISABLED;
        }

        p_ctx->evt_handler(evt);
    }
}


void nrf_ble_amts_on_ble_evt(nrf_ble_amts_t * p_ctx, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_ctx, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_ctx, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_ctx, p_ble_evt);
            break;

        case BLE_EVT_TX_COMPLETE:
            on_tx_complete(p_ctx);
            break;

        default:
            break;
    }
}


void nrf_ble_amts_init(nrf_ble_amts_t * p_ctx, amts_evt_handler_t evt_handler)
{
    ret_code_t    err_code;
    uint16_t      service_handle;
    ble_uuid_t    ble_uuid;
    ble_uuid128_t base_uuid = {SERVICE_UUID_BASE};

    err_code = sd_ble_uuid_vs_add(&base_uuid, &(p_ctx->uuid_type));
    APP_ERROR_CHECK(err_code);

    ble_uuid.type = p_ctx->uuid_type;
    ble_uuid.uuid = AMT_SERVICE_UUID;

    // Add service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &service_handle);
    APP_ERROR_CHECK(err_code);

    // Add AMTS characteristic.
    ble_add_char_params_t amt_params;
    memset(&amt_params, 0, sizeof(amt_params));

    amt_params.uuid              = AMTS_CHAR_UUID;
    amt_params.uuid_type         = p_ctx->uuid_type;
    amt_params.max_len           = NRF_BLE_GATT_MAX_MTU_SIZE;
    amt_params.char_props.notify = 1;
    amt_params.cccd_write_access = SEC_OPEN;
    amt_params.is_var_len        = 1;

    err_code = characteristic_add(service_handle, &amt_params, &(p_ctx->amts_char_handles));
    APP_ERROR_CHECK(err_code);

    // Add AMT Received Bytes Count characteristic.
    ble_add_char_params_t amt_rbc_params;
    memset(&amt_rbc_params, 0, sizeof(amt_rbc_params));

    amt_rbc_params.uuid            = AMT_RCV_BYTES_CNT_CHAR_UUID;
    amt_rbc_params.uuid_type       = p_ctx->uuid_type;
    amt_rbc_params.max_len         = AMT_RCV_BYTES_CNT_MAX_LEN;
    amt_rbc_params.char_props.read = 1;
    amt_rbc_params.read_access     = SEC_OPEN;

    err_code = characteristic_add(service_handle, &amt_rbc_params, &(p_ctx->amt_rbc_char_handles));
    APP_ERROR_CHECK(err_code);

    p_ctx->evt_handler = evt_handler;
}


void nrf_ble_amts_notif_spam(nrf_ble_amts_t * p_ctx)
{
    p_ctx->kbytes_sent = 0;
    p_ctx->bytes_sent  = 0;
    char_notification_send(p_ctx);
}


void nrf_ble_amts_on_gatt_evt(nrf_ble_amts_t * p_ctx, nrf_ble_gatt_evt_t * p_gatt_evt)
{
    p_ctx->max_payload_len = p_gatt_evt->att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
}


static void char_notification_send(nrf_ble_amts_t * p_ctx)
{
    uint8_t            data[256];
    uint16_t           len = p_ctx->max_payload_len;
    nrf_ble_amts_evt_t evt;

    if (p_ctx->bytes_sent >= AMT_BYTE_TRANSFER_CNT)
    {
        evt.bytes_transfered_cnt = p_ctx->bytes_sent;
        p_ctx->busy              = false;
        p_ctx->bytes_sent        = 0;
        p_ctx->kbytes_sent       = 0;

        evt.evt_type = SERVICE_EVT_TRANSFER_FINISHED;
        p_ctx->evt_handler(evt);
        return;
    }

    ble_gatts_hvx_params_t const hvx_param =
    {
        .type   = BLE_GATT_HVX_NOTIFICATION,
        .handle = p_ctx->amts_char_handles.value_handle,
        .p_data = data,
        .p_len  = &len,
    };

    uint32_t err_code = NRF_SUCCESS;
    while (err_code == NRF_SUCCESS)
    {
        (void) uint32_encode(p_ctx->bytes_sent, data);

        err_code = sd_ble_gatts_hvx(p_ctx->conn_handle, &hvx_param);

        if (err_code == BLE_ERROR_NO_TX_PACKETS)
        {
            // Wait for BLE_EVT_TX_COMPLETE.
            p_ctx->busy = true;
            break;
        }
        else if (err_code != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("sd_ble_gatts_hvx() failed: 0x%x\r\n", err_code);
        }

        p_ctx->bytes_sent += len;

        if (p_ctx->kbytes_sent != (p_ctx->bytes_sent / 1024))
        {
            p_ctx->kbytes_sent = (p_ctx->bytes_sent / 1024);

            evt.evt_type             = SERVICE_EVT_TRANSFER_1KB;
            evt.bytes_transfered_cnt = p_ctx->bytes_sent;
            p_ctx->evt_handler(evt);
        }
    }
}


void nrf_ble_amts_rbc_set(nrf_ble_amts_t * p_ctx, uint32_t byte_cnt)
{
    uint8_t  data[AMT_RCV_BYTES_CNT_MAX_LEN];
    uint16_t len;

    ble_gatts_value_t value_param;

    memset(&value_param, 0x00, sizeof(value_param));

    len = (uint16_t)uint32_encode(byte_cnt, data);
    value_param.len     = len;
    value_param.p_value = data;

    ret_code_t err_code = sd_ble_gatts_value_set(p_ctx->conn_handle,
                                                 p_ctx->amt_rbc_char_handles.value_handle,
                                                 &value_param);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("sd_ble_gatts_value_set() failed: 0x%x\r\n", err_code);
    }
}

/** @}
 *  @endcond
 */
