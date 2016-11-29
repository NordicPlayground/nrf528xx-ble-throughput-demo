/* Copyright (c) 2015 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#ifndef DRV_DISP_ENGINE_H__
#define DRV_DISP_ENGINE_H__

#include "fb.h"

#include <stdint.h>
#include <stdbool.h>

/**@brief The display engine status codes.
 */
enum
{
    DRV_DISP_ENGINE_STATUS_CODE_SUCCESS,       ///< Successfull.
    DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED,    ///< Disallowed.
    DRV_DISP_ENGINE_STATUS_CODE_INVALID_PARAM, ///< Invalid parameters.
};

typedef enum
{
    DRV_DISP_ENGINE_PROC_NONE = 0,      ///< No procedure.
    DRV_DISP_ENGINE_PROC_CLEAR,         ///< Procedure to clear the display.
    DRV_DISP_ENGINE_PROC_WRITE,         ///< Procedure to write to the display.
    DRV_DISP_ENGINE_PROC_READ,          ///< Procedure to read from the display.
    DRV_DISP_ENGINE_PROC_UPDATE,        ///< Procedure to update all changed lines.
    DRV_DISP_ENGINE_PROC_TOFBCPY,       ///< Procedure to copy to the framebuffer to the display.
    DRV_DISP_ENGINE_PROC_FROMFBCPY,     ///< Procedure to copy to the display from the framebuffer.
} drv_disp_engine_proc_type_t;



/**@brief The display engine configuration.
 */
typedef struct
{
    fb_next_dirty_line_get_ptr_t  fb_next_dirty_line_get;   ///< Pointer to the next dirty line get function.
    fb_line_storage_ptr_get_ptr_t fb_line_storage_ptr_get;  ///< Pointer to the line storage pointer get function.
    fb_line_storage_set_ptr_t     fb_line_storage_set;      ///< Pointer to the line storage set function.
} drv_disp_engine_cfg_t;



/**@brief The display engine drive type definition.
 */
typedef enum
{
    DRV_DISP_ENGINE_PROC_DRIVE_TYPE_TICK = 0, ///< Drives the display engine one step.
    DRV_DISP_ENGINE_PROC_DRIVE_TYPE_BLOCKING, ///< Drives the display engine to the end.
} drv_disp_engine_proc_drive_type_t;


/**@brief The display engine access type definition.
 */
typedef enum
{
    DRV_DISP_ENGINE_PROC_ACCESS_TYPE_PREAMBLE = 0, ///< Preamble access descriptor is used for access.
    DRV_DISP_ENGINE_PROC_ACCESS_TYPE_DATA,         ///< Data descriptor is used for access.
    DRV_DISP_ENGINE_PROC_ACCESS_TYPE_POSTAMBLE,    ///< Postamble descriptor is used for access.
} drv_disp_engine_proc_access_type_t;


/**@brief The display engine drive exit status definition.
 */
typedef enum
{
    DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_ACTIVE,      ///< Procedure active.
    DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_COMPLETE,    ///< Procedure complete.
    DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_PAUSE,       ///< Procedure pause.
    DRV_DISP_ENGINE_PROC_DRIVE_EXIT_STATUS_ERROR,       ///< Procedure complete.
} drv_disp_engine_proc_drive_exit_status_t;


/**@brief The display engine drive information definition. */
typedef struct
{
    drv_disp_engine_proc_type_t                 proc_type;
    drv_disp_engine_proc_drive_exit_status_t    exit_status;
} drv_disp_engine_proc_drive_info_t;


/**@brief The display engine access descriptor definition. */
typedef struct
{
    struct
    {
        uint8_t *   p_preamble;                 ///< The location of the preamble storage.
        uint8_t *   p_data;                     ///< The location of the data storage.
        uint8_t *   p_postamble;                ///< The location of the postamble storage.
        uint8_t     preamble_length;            ///< The length (or max length when reading) of the preamble storage.
        uint8_t     data_length;                ///< The length (or max length when reading) of the data storage.
        uint8_t     postamble_length;           ///< The length (or max length when reading) of the postamble storage.
    } buffers;
    drv_disp_engine_proc_type_t current_proc;   ///< The currently running display engine procedure.
} drv_disp_engine_access_descr_t;


/**@brief The access begin function definition.
 *
 * @param p_access_descr    The location of the access descriptior.
 *
 * @param new_proc          The new procedure to be initiated. */
typedef uint32_t (*drv_disp_engine_access_begin_t) (drv_disp_engine_access_descr_t *p_access_descr, drv_disp_engine_proc_type_t new_proc);


/**@brief The access update function definition.
 *
 * @param p_access_descr    The location of the access descriptior. */
typedef bool     (*drv_disp_engine_access_update_t) (drv_disp_engine_access_descr_t *p_access_descr);


/**@brief The write function definition.
 *
 * @param access_type   The access type.
 * @param line_number   The line number to write to.
 * @param p_buf         The location of the data to write.
 * @param length        The length of the data to write.*/
typedef uint32_t (*drv_disp_engine_line_write_t) (drv_disp_engine_proc_access_type_t access_type, uint16_t line_number, uint8_t *p_buf, uint8_t length);


/**@brief The write function definition.
 *
 * @param access_type   The access type.
 * @param line_number   The line number to read from.
 * @param p_buf         The location of where the data is to be stored.
 * @param length        The length of the data to read.*/
typedef uint32_t (*drv_disp_engine_line_read_t) (drv_disp_engine_proc_access_type_t access_type, uint8_t *p_buf, uint16_t line_number, uint8_t length);


/**@brief The access end function definition. */
typedef uint32_t (*drv_disp_engine_access_end_t) (void);


typedef struct
{
    drv_disp_engine_access_begin_t  access_begin;   ///< The function to be called by the display engine when an access begins.
    drv_disp_engine_line_write_t    line_write;     ///< The function to be called by the display engine is writing a buffer.
    drv_disp_engine_line_read_t     line_read;      ///< The function to be called by the display engine is reading a buffer.
    drv_disp_engine_access_update_t access_update;  ///< The function to be called by the display engine when the access descriptor can be updated.
    drv_disp_engine_access_end_t    access_end;     ///< The function to be called by the display engine when an access ends.
} drv_disp_engine_user_cfg_t;


/**@brief Initializes the display engine.
 *
 * @param p_drv_disp_engine_cfg   The location of the display engine configuration. */
uint32_t drv_disp_engine_init(drv_disp_engine_cfg_t const * const p_drv_disp_engine_cfg);


/**@brief Initializes the display engine.
 *
 * @param p_user_cfg            The location of the user configuration.
 * @param proc                  The display engine procedure to start.
 * @param p_access_descr[out]   The location of the pointer to the access descriptor, or NULL if it should not be updated. 
 *
 * @retval DRV_DISP_ENGINE_STATUS_CODE_SUCCESS      If the procedure has been initiated.
 * @retval DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED   If not allowed at this time. */
uint32_t drv_disp_engine_proc_initiate(drv_disp_engine_user_cfg_t *p_user_cfg, drv_disp_engine_proc_type_t proc, drv_disp_engine_access_descr_t ** p_access_descr);


/**@brief Initializes the display engine.
 *
 * @param drive_type            The way to drive the procedure.
 * @param p_drive_info          The location where the drive information is to be stored, or NULL if it should not be stored.
 * @param p_access_descr[out]   The location of the pointer to the access descriptor. 
 *
 * @retval DRV_DISP_ENGINE_STATUS_CODE_SUCCESS      If successful.
 * @retval DRV_DISP_ENGINE_STATUS_CODE_DISALLOWED   If not allowed at this time. */
uint32_t drv_disp_engine_proc_drive(drv_disp_engine_proc_drive_type_t drive_type, drv_disp_engine_proc_drive_info_t *drive_info);

#endif // DRV_DISP_ENGINE_H__
