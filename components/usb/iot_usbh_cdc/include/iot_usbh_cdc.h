/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "usb/usb_host.h"

#ifdef __cplusplus
extern "C" {
#endif

// Use these macros to match any VID or PID when setting up a CDC device
#define CDC_HOST_ANY_VID (0)
#define CDC_HOST_ANY_PID (0)

// Handle to a CDC device, used for communication with the USB device
typedef struct usbh_cdc_t *usbh_cdc_handle_t;

/**
 * @brief New USB device callback
 *
 * Provides already opened usb_dev, that will be closed after this callback returns.
 * This is useful for peeking device's descriptors, e.g. peeking VID/PID and loading proper driver.
 *
 * @attention This callback is called from USB Host context, so the CDC device can't be opened here.
 *
 */
typedef void (*usbh_cdc_new_dev_cb_t)(usb_device_handle_t usb_dev);

typedef struct {
    size_t task_stack_size;                    /*!< Stack size of the driver's task */
    unsigned task_priority;                    /*!< Priority of the driver's task */
    int task_coreid;                           /*!< Core of the driver's task, Set it to -1 to not specify the core. */
    bool skip_init_usb_host_driver;            /*!< Skip initialization of USB host driver */
    usbh_cdc_new_dev_cb_t new_dev_cb;          /*!< Callback function when a new device is connected */
} usbh_cdc_driver_config_t;

/**
 * @brief Callback structure for CDC device events
 */
typedef void (*usbh_cdc_event_cb_t)(usbh_cdc_handle_t cdc_handle, void *user_data);

typedef struct {
    usbh_cdc_event_cb_t connect;            /*!< USB connect callback, set NULL if use */
    usbh_cdc_event_cb_t disconnect;         /*!< USB disconnect callback, set NULL if not use */
    usbh_cdc_event_cb_t revc_data;          /*!< USB receive data callback, set NULL if not use */
    void *user_data;                        /*!< Pointer to user data that will be passed to the callbacks */
} usbh_cdc_event_callbacks_t;

/**
 * @brief Configuration structure for initializing a USB CDC device
 */
typedef struct usbh_cdc_config {
    uint16_t vid;                           /*!< Vendor ID: If set, the `pid` parameter must be configured
                                                 If not set, it will default to opening the first connected device */
    uint16_t pid;                           /*!< Product ID: If set, the `vid` parameter must be configured
                                                 If not set, it will default to opening the first connected device */
    int itf_num;                            /*!< interface numbers */
    size_t rx_buffer_size;                  /*!< Size of the receive buffer, default is 1024 bytes if set to 0 */
    size_t tx_buffer_size;                  /*!< Size of the transmit buffer, default is 1024 bytes if set to 0 */
    usbh_cdc_event_callbacks_t cbs;         /*!< Event callbacks for the CDC device */
} usbh_cdc_device_config_t;

/**
 * @brief Install the USB CDC driver
 *
 * This function installs and initializes the USB CDC driver. It sets up internal data structures, creates necessary tasks,
 * and registers the USB host client. If the USB host driver is not already initialized, it will create and initialize it.
 *
 * @param config Pointer to a configuration structure (`usbh_cdc_driver_config_t`) that specifies the driver's settings such as task stack size, priority, core affinity, and new device callback.
 *
 * @return
 *     - ESP_OK: Driver installed successfully
 *     - ESP_ERR_INVALID_ARG: Null configuration parameter or invalid arguments
 *     - ESP_ERR_INVALID_STATE: USB CDC driver already installed
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 *     - ESP_FAIL: Failed to create necessary tasks or USB host installation failed
 *
 * @note If the `skip_init_usb_host_driver` flag in the config is set to true, the function will skip the USB host driver initialization.
 *
 * The function performs the following steps:
 * 1. Verifies input arguments and state.
 * 2. Optionally initializes the USB host driver.
 * 3. Allocates memory for the USB CDC object.
 * 4. Creates necessary synchronization primitives (event group, mutex).
 * 5. Creates the driver task.
 * 6. Registers the USB host client with the provided configuration.
 * 7. Initializes the CDC driver structure and resumes the driver task.
 *
 * In case of an error, the function will clean up any resources that were allocated before the failure occurred.
 */
esp_err_t usbh_cdc_install(const usbh_cdc_driver_config_t *config);

/**
 * @brief Uninstall the USB CDC driver
 *
 * This function uninstalls the USB CDC driver, releasing any allocated resources and stopping all driver-related tasks.
 * It ensures that all CDC devices are closed before proceeding with the uninstallation.
 *
 * @return
 *     - ESP_OK: Driver uninstalled successfully
 *     - ESP_ERR_INVALID_STATE: Driver is not installed or devices are still active
 *     - ESP_ERR_NOT_FINISHED: Timeout occurred while waiting for the CDC task to finish
 */
esp_err_t usbh_cdc_uninstall(void);

/**
 * @brief Create a new USB CDC device handle
 *
 * This function allocates and initializes a new USB CDC device based on the provided configuration, including setting up the ring buffers
 * for data transmission and reception, and searching for and opening the appropriate USB device.
 *
 * @param[out] cdc_handle Pointer to a variable that will hold the created CDC device handle upon successful creation
 * @param[in] config Pointer to a `usbh_cdc_device_config_t` structure containing the configuration for the new device, including vendor ID (VID), product ID (PID), and buffer sizes
 *
 * @return
 *     - ESP_OK: CDC device successfully created
 *     - ESP_ERR_INVALID_STATE: USB CDC driver is not installed
 *     - ESP_ERR_INVALID_ARG: Null `cdc_handle` or `config` parameter
 *     - ESP_ERR_NO_MEM: Memory allocation failed or ring buffer creation failed
 *     - ESP_FAIL: Failed to open the CDC device
 */
esp_err_t usbh_cdc_create(const usbh_cdc_device_config_t *config, usbh_cdc_handle_t *cdc_handle);

/**
 * @brief Delete a USB CDC device handle
 *
 * This function deletes the specified USB CDC device handle, freeing its associated resources, including ring buffers and semaphores.
 *
 * @param[in] cdc_handle The CDC device handle to delete
 *
 * @return
 *     - ESP_OK: CDC device deleted successfully
 *     - ESP_ERR_INVALID_STATE: USB CDC driver is not installed
 *     - ESP_ERR_INVALID_ARG: Invalid CDC handle provided
 */
esp_err_t usbh_cdc_delete(usbh_cdc_handle_t cdc_handle);

/**
 * @brief Write data to the USB CDC device
 *
 * This function writes data to the specified USB CDC device by pushing the data into the output ring buffer.
 * If the buffer is full or the device is not connected, the write will fail.
 *
 * @param[in] cdc_handle The CDC device handle
 * @param[in] buf Pointer to the data buffer to write
 * @param[in out] length Pointer to the length of data to write. On success, it remains unchanged, otherwise set to 0.
 *
 * @return
 *     - ESP_OK: Data written successfully
 *     - ESP_ERR_INVALID_ARG: Invalid argument (NULL handle, buffer, or length)
 *     - ESP_ERR_INVALID_STATE: Device is not connected
 */
esp_err_t usbh_cdc_write_bytes(usbh_cdc_handle_t cdc_handle, const uint8_t *buf, size_t *length);

/**
 * @brief Read data from the USB CDC device
 *
 * This function reads data from the specified USB CDC device by popping data from the input ring buffer.
 * If no data is available or the device is not connected, the read will fail.
 *
 * @param[in] cdc_handle The CDC device handle
 * @param[out] buf Pointer to the buffer where the read data will be stored
 * @param[in out] length Pointer to the length of data to read. On success, it is updated with the actual bytes read.
 *
 * @return
 *     - ESP_OK: Data read successfully
 *     - ESP_ERR_INVALID_ARG: Invalid argument (NULL handle, buffer, or length)
 *     - ESP_ERR_INVALID_STATE: Device is not connected
 */
esp_err_t usbh_cdc_read_bytes(usbh_cdc_handle_t cdc_handle, const uint8_t *buf, size_t *length);

/**
 * @brief Flush the receive buffer of the USB CDC device
 *
 * This function clears the receive buffer, discarding any data currently in the buffer.
 *
 * @param[in] cdc_handle The CDC device handle
 *
 * @return
 *     - ESP_OK: Receive buffer flushed successfully
 *     - ESP_ERR_INVALID_ARG: Invalid CDC handle provided
 */
esp_err_t usbh_cdc_flush_rx_buffer(usbh_cdc_handle_t cdc_handle);

/**
 * @brief Flush the transmit buffer of the USB CDC device
 *
 * This function clears the transmit buffer, discarding any data currently in the buffer.
 *
 * @param[in] cdc_handle The CDC device handle
 *
 * @return
 *     - ESP_OK: Transmit buffer flushed successfully
 *     - ESP_ERR_INVALID_ARG: Invalid CDC handle provided
 */
esp_err_t usbh_cdc_flush_tx_buffer(usbh_cdc_handle_t cdc_handle);

/**
 * @brief Get the size of the receive buffer of the USB CDC device
 *
 * This function retrieves the current size of the receive buffer in bytes.
 *
 * @param[in] cdc_handle The CDC device handle
 * @param[out] size Pointer to store the size of the receive buffer
 *
 * @return
 *     - ESP_OK: Size retrieved successfully
 *     - ESP_ERR_INVALID_ARG: Invalid CDC handle provided
 */
esp_err_t usbh_cdc_get_rx_buffer_size(usbh_cdc_handle_t cdc_handle, size_t *size);

/**
 * @brief Get the connect state of given interface
 *
 * @param[in] cdc_handle The CDC device handle
 *
 * @return
 *      -1: error
 *      0 : disconnected
 *      1 : connected
 */
int usbh_cdc_get_state(usbh_cdc_handle_t cdc_handle);

/**
 * @brief Print the USB CDC device descriptors
 *
 * This function retrieves and prints the USB device and configuration descriptors
 * for the specified USB CDC device. It checks that the device is properly connected
 * and open before retrieving and printing the descriptors.
 *
 * @param[in] cdc_handle The CDC device handle
 *
 * @return
 *     - ESP_OK: Descriptors printed successfully
 *     - ESP_ERR_INVALID_ARG: Invalid CDC handle provided
 *     - ESP_ERR_INVALID_STATE: Device is not connected or not yet open
 */
esp_err_t usbh_cdc_desc_print(usbh_cdc_handle_t cdc_handle);

#ifdef __cplusplus
#endif
