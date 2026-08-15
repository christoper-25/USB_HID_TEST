#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"

#include "usb/usb_host.h"
#include "usb/hid_host.h"

#define TAG "SOIL_SENSOR"

#define SENSOR_VID 0x0487
#define SENSOR_PID 0x0007

#define HID_REPORT_SIZE 64

typedef enum {
    HID_EVENT_CONNECTED,
    HID_EVENT_INPUT,
    HID_EVENT_DISCONNECTED
} hid_event_type_t;

typedef struct {
    hid_event_type_t type;

    hid_host_device_handle_t handle;

    uint8_t data[HID_REPORT_SIZE];

    size_t length;

} hid_event_t;

static QueueHandle_t hid_event_queue = NULL;


/* =========================================================
 * PRINT HEX
 * ========================================================= */

static void print_hex(
    const uint8_t *data,
    size_t length
)
{
    for (size_t i = 0; i < length; i++) {

        printf("%02X ", data[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    if (length % 16 != 0) {
        printf("\n");
    }
}


/* =========================================================
 * SEND SENSOR COMMAND
 *
 * Uses HID SET_REPORT.
 *
 * Report descriptor:
 *
 * 81 02 = Input report
 * 91 02 = Output report
 *
 * Therefore report type = OUTPUT.
 * Report ID = 0.
 *
 * ========================================================= */

static esp_err_t send_realtime_command(
    hid_host_device_handle_t device
)
{
    uint8_t command[64] = {0};

    /*
     * Sensor command
     *
     * 55 01 22
     */

    command[0] = 0x55;
    command[1] = 0x01;
    command[2] = 0x22;

    /*
     * Command checksum / ending bytes
     */

    command[62] = 0xE7;
    command[63] = 0x46;


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "REAL-TIME SENSOR COMMAND"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    ESP_LOGI(
        TAG,
        "COMMAND (64 bytes):"
    );

    print_hex(
        command,
        sizeof(command)
    );


    ESP_LOGI(
        TAG,
        "Sending HID SET_REPORT..."
    );


    /*
     * HID output report
     *
     * HID report type:
     * HID_REPORT_TYPE_OUTPUT
     *
     * Report ID:
     * 0
     */

    esp_err_t ret =
        hid_class_request_set_report(
            device,
            HID_REPORT_TYPE_OUTPUT,
            0,
            command,
            sizeof(command)
        );


    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "HID SET_REPORT failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "SENSOR COMMAND SENT SUCCESSFULLY"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    return ESP_OK;
}


/* =========================================================
 * HID INTERFACE CALLBACK
 * ========================================================= */

static void hid_interface_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_interface_event_t event,
    void *arg
)
{
    (void)arg;


    /* -----------------------------------------------------
     * INPUT REPORT
     * ----------------------------------------------------- */

    if (
        event ==
        HID_HOST_INTERFACE_EVENT_INPUT_REPORT
    ) {

        uint8_t report[HID_REPORT_SIZE];

        size_t report_length = 0;


        esp_err_t err =
            hid_host_device_get_raw_input_report_data(
                hid_device_handle,
                report,
                sizeof(report),
                &report_length
            );


        if (err != ESP_OK) {

            ESP_LOGE(
                TAG,
                "Failed to get HID input report: %s",
                esp_err_to_name(err)
            );

            return;
        }


        hid_event_t queue_event;

        memset(
            &queue_event,
            0,
            sizeof(queue_event)
        );


        queue_event.type =
            HID_EVENT_INPUT;

        queue_event.handle =
            hid_device_handle;

        queue_event.length =
            report_length;


        if (
            queue_event.length >
            HID_REPORT_SIZE
        ) {

            queue_event.length =
                HID_REPORT_SIZE;
        }


        memcpy(
            queue_event.data,
            report,
            queue_event.length
        );


        if (hid_event_queue != NULL) {

            xQueueSend(
                hid_event_queue,
                &queue_event,
                0
            );
        }


        return;
    }


    /* -----------------------------------------------------
     * TRANSFER ERROR
     * ----------------------------------------------------- */

    if (
        event ==
        HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR
    ) {

        ESP_LOGE(
            TAG,
            "HID transfer error"
        );

        return;
    }


    /* -----------------------------------------------------
     * DISCONNECTED
     * ----------------------------------------------------- */

    if (
        event ==
        HID_HOST_INTERFACE_EVENT_DISCONNECTED
    ) {

        hid_event_t queue_event;

        memset(
            &queue_event,
            0,
            sizeof(queue_event)
        );


        queue_event.type =
            HID_EVENT_DISCONNECTED;

        queue_event.handle =
            hid_device_handle;


        if (hid_event_queue != NULL) {

            xQueueSend(
                hid_event_queue,
                &queue_event,
                0
            );
        }


        return;
    }
}


/* =========================================================
 * HID DRIVER CALLBACK
 * ========================================================= */

static void hid_driver_callback(
    hid_host_device_handle_t hid_device_handle,
    const hid_host_driver_event_t event,
    void *arg
)
{
    (void)arg;


    if (
        event !=
        HID_HOST_DRIVER_EVENT_CONNECTED
    ) {

        return;
    }


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "HID DEVICE CONNECTED"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    hid_event_t queue_event;

    memset(
        &queue_event,
        0,
        sizeof(queue_event)
    );


    queue_event.type =
        HID_EVENT_CONNECTED;

    queue_event.handle =
        hid_device_handle;


    if (hid_event_queue != NULL) {

        xQueueSend(
            hid_event_queue,
            &queue_event,
            portMAX_DELAY
        );
    }
}


/* =========================================================
 * USB HOST TASK
 * ========================================================= */

static void usb_host_task(
    void *arg
)
{
    (void)arg;


    const usb_host_config_t host_config = {

        .skip_phy_setup = false,

        .intr_flags =
            ESP_INTR_FLAG_LEVEL1
    };


    ESP_ERROR_CHECK(
        usb_host_install(
            &host_config
        )
    );


    ESP_LOGI(
        TAG,
        "USB Host installed"
    );


    while (true) {

        uint32_t event_flags = 0;


        esp_err_t err =
            usb_host_lib_handle_events(
                portMAX_DELAY,
                &event_flags
            );


        if (err != ESP_OK) {

            ESP_LOGE(
                TAG,
                "USB host event error: %s",
                esp_err_to_name(err)
            );
        }
    }
}


/* =========================================================
 * OPEN HID DEVICE
 * ========================================================= */

static void open_hid_device(
    hid_host_device_handle_t device
)
{
    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "OPENING HID DEVICE"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    /* =====================================================
     * DEVICE INFORMATION
     * ===================================================== */

    hid_host_dev_info_t info;

    memset(
        &info,
        0,
        sizeof(info)
    );


    esp_err_t err =
        hid_host_get_device_info(
            device,
            &info
        );


    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "hid_host_get_device_info failed: %s",
            esp_err_to_name(err)
        );

        return;
    }


    ESP_LOGI(
        TAG,
        "VID: %04X",
        info.VID
    );

    ESP_LOGI(
        TAG,
        "PID: %04X",
        info.PID
    );


    /* =====================================================
     * HID PARAMETERS
     * ===================================================== */

    hid_host_dev_params_t params;

    memset(
        &params,
        0,
        sizeof(params)
    );


    err =
        hid_host_device_get_params(
            device,
            &params
        );


    if (err == ESP_OK) {

        ESP_LOGI(
            TAG,
            "Interface : %u",
            params.iface_num
        );

        ESP_LOGI(
            TAG,
            "Subclass  : %u",
            params.sub_class
        );

        ESP_LOGI(
            TAG,
            "Protocol  : %u",
            params.proto
        );

        ESP_LOGI(
            TAG,
            "Address   : %u",
            params.addr
        );
    }


    /* =====================================================
     * CHECK SENSOR
     * ===================================================== */

    if (
        info.VID != SENSOR_VID ||
        info.PID != SENSOR_PID
    ) {

        ESP_LOGW(
            TAG,
            "Unknown HID device"
        );

        return;
    }


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "SOIL SENSOR DETECTED!"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    /* =====================================================
     * OPEN HID INTERFACE
     * ===================================================== */

    const hid_host_device_config_t device_config = {

        .callback =
            hid_interface_callback,

        .callback_arg =
            NULL
    };


    err =
        hid_host_device_open(
            device,
            &device_config
        );


    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "hid_host_device_open failed: %s",
            esp_err_to_name(err)
        );

        return;
    }


    ESP_LOGI(
        TAG,
        "HID interface opened successfully"
    );


    ESP_LOGI(
        TAG,
        "Interface : %u",
        params.iface_num
    );

    ESP_LOGI(
        TAG,
        "Subclass  : %u",
        params.sub_class
    );

    ESP_LOGI(
        TAG,
        "Protocol  : %u",
        params.proto
    );

    ESP_LOGI(
        TAG,
        "Address   : %u",
        params.addr
    );


    /* =====================================================
     * REPORT DESCRIPTOR
     * ===================================================== */

    size_t descriptor_length = 0;


    uint8_t *descriptor =
        hid_host_get_report_descriptor(
            device,
            &descriptor_length
        );


    if (descriptor != NULL) {

        ESP_LOGI(
            TAG,
            "Report descriptor length: %zu",
            descriptor_length
        );


        printf(
            "\nREPORT DESCRIPTOR (%zu bytes):\n",
            descriptor_length
        );


        print_hex(
            descriptor,
            descriptor_length
        );
    }


    /* =====================================================
     * START INPUT REPORTING
     * ===================================================== */

    err =
        hid_host_device_start(
            device
        );


    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "hid_host_device_start failed: %s",
            esp_err_to_name(err)
        );


        hid_host_device_close(
            device
        );

        return;
    }


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "HID INPUT REPORTING STARTED"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    /*
     * Give the HID interface time to become active.
     */

    vTaskDelay(
        pdMS_TO_TICKS(1000)
    );


    /* =====================================================
     * SEND SENSOR COMMAND
     * ===================================================== */

    send_realtime_command(
        device
    );


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "LISTENING FOR SENSOR RESPONSES"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );
}


/* =========================================================
 * APPLICATION MAIN
 * ========================================================= */

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "ESP32-S3 USB HID SOIL SENSOR TEST"
    );

    ESP_LOGI(
        TAG,
        "ESP-IDF 5.5.5"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    ESP_LOGI(
        TAG,
        "Expected VID: 0x%04X",
        SENSOR_VID
    );

    ESP_LOGI(
        TAG,
        "Expected PID: 0x%04X",
        SENSOR_PID
    );


    /* =====================================================
     * CREATE QUEUE
     * ===================================================== */

    hid_event_queue =
        xQueueCreate(
            20,
            sizeof(hid_event_t)
        );


    if (hid_event_queue == NULL) {

        ESP_LOGE(
            TAG,
            "Failed to create HID event queue"
        );

        return;
    }


    /* =====================================================
     * START USB HOST
     * ===================================================== */

    BaseType_t task_result =
        xTaskCreatePinnedToCore(
            usb_host_task,
            "usb_host",
            4096,
            NULL,
            5,
            NULL,
            0
        );


    if (task_result != pdPASS) {

        ESP_LOGE(
            TAG,
            "Failed to create USB host task"
        );

        return;
    }


    /* =====================================================
     * WAIT FOR USB HOST
     * ===================================================== */

    vTaskDelay(
        pdMS_TO_TICKS(500)
    );


    /* =====================================================
     * INSTALL HID HOST DRIVER
     * ===================================================== */

    const hid_host_driver_config_t driver_config = {

        .create_background_task = true,

        .task_priority = 5,

        .stack_size = 4096,

        .core_id = 0,

        .callback =
            hid_driver_callback,

        .callback_arg =
            NULL
    };


    ESP_ERROR_CHECK(
        hid_host_install(
            &driver_config
        )
    );


    ESP_LOGI(
        TAG,
        "HID Host driver installed"
    );


    ESP_LOGI(
        TAG,
        "================================================"
    );

    ESP_LOGI(
        TAG,
        "CONNECT USB SOIL SENSOR NOW"
    );

    ESP_LOGI(
        TAG,
        "================================================"
    );


    /* =====================================================
     * MAIN EVENT LOOP
     * ===================================================== */

    while (true) {

        hid_event_t event;


        if (
            xQueueReceive(
                hid_event_queue,
                &event,
                portMAX_DELAY
            )
        ) {

            /* ---------------------------------------------
             * CONNECTED
             * --------------------------------------------- */

            if (
                event.type ==
                HID_EVENT_CONNECTED
            ) {

                open_hid_device(
                    event.handle
                );
            }


            /* ---------------------------------------------
             * INPUT REPORT
             * --------------------------------------------- */

            else if (
                event.type ==
                HID_EVENT_INPUT
            ) {

                ESP_LOGI(
                    TAG,
                    "================================================"
                );

                ESP_LOGI(
                    TAG,
                    "HID INPUT REPORT RECEIVED"
                );

                ESP_LOGI(
                    TAG,
                    "Length: %zu bytes",
                    event.length
                );

                ESP_LOGI(
                    TAG,
                    "================================================"
                );


                print_hex(
                    event.data,
                    event.length
                );


                ESP_LOGI(
                    TAG,
                    "================================================"
                );
            }


            /* ---------------------------------------------
             * DISCONNECTED
             * --------------------------------------------- */

            else if (
                event.type ==
                HID_EVENT_DISCONNECTED
            ) {

                ESP_LOGW(
                    TAG,
                    "================================================"
                );

                ESP_LOGW(
                    TAG,
                    "USB HID DEVICE DISCONNECTED"
                );

                ESP_LOGW(
                    TAG,
                    "================================================"
                );
            }
        }
    }
}