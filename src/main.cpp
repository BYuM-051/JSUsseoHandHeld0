// NOTE : I know that you wont read this sentence :/
#include <RTOS.h>
#define __DEBUG__

#define CurrentBoard BH1
// arduino core ============================================================
#include <Arduino.h>
void setup();
void loop();

// Project BOARD Info ======================================================
#define MAX_BOARDS 6
#define BR0 0
#define BR1 1
#define BR2 2
#define BH0 3
#define BH1 4
#define BS0 5

//아씨 뭐하려고 했었지 이거
#if (CurrentBoard) == BR0
#elif (CurrentBoard) == BR1
#elif (CurrentBoard) == BR2
#elif (CurrentBoard) == BH0
#elif (CurrentBoard) == BH1
#elif (CurrentBoard) == BS0
#else
#endif

constexpr uint8_t MAC_ADDR [MAX_BOARDS][6] = 
{
    {0x20, 0x43, 0xA8, 0x42, 0x0C, 0xC8}, // BR0
    {0xC0, 0x5D, 0x89, 0xE9, 0x1C, 0x30}, // BR1
    {0x20, 0x43, 0xA8, 0x42, 0x0C, 0xCC}, // BR2
    {0x20, 0x43, 0xA8, 0x42, 0x10, 0xEC}, // BH0
    {0x3C, 0x84, 0x27, 0xFC, 0xD8, 0x94}, // BH1
    {0xC0, 0x5D, 0x89, 0xE9, 0x1C, 0x44}  // BS0
};

constexpr uint16_t UWB_ID [MAX_BOARDS] = 
{
    0x100,  // BR0
    0x101,  // BR1
    0x102,  // BR2
    0x200,  // BH0
    0x000,  // BH1 (N/A)
    0x300   // BS0
};

// UART Communication ======================================================
#include "driver/uart.h"
#define MAX_SERIAL_LENGTH 64
#define UART_MESSAGE_QUEUE_DEPTH 10
#define UART_RX_BUFFER_SIZE (MAX_SERIAL_LENGTH * UART_MESSAGE_QUEUE_DEPTH)
#define UART_TX_BUFFER_SIZE (MAX_SERIAL_LENGTH * 2)
#define UART_EVENT_QUEUE_SIZE 20
#define HHUWB UART_NUM_1
#define UART1_RX_PIN GPIO_NUM_16
#define UART1_TX_PIN GPIO_NUM_17
#define UART_LISTENER_CORE 0
#define UART_BAUD_RATE 115200
#define UART_BLOCK_TICKS pdMS_TO_TICKS(portMAX_DELAY)

QueueHandle_t hhuwbEventQueue;

volatile bool isConnected = false;
volatile TickType_t lastBRUpdateTime = 0;
volatile TickType_t lastBSUpdateTime = 0;
volatile uint32_t distanceBetweenBHAndBR = 0;
volatile uint32_t distanceBetweenBHAndBS = 0;

void uart_init(void);
void uartListener(void *param);
void onUARTDataReceived(char *cmdText);
void uartPrintf(uart_port_t uart_num, const char *format, ...);

#ifdef __DEBUG__
    volatile int pingCount = 0;
    #define MAX_PING_COUNT 5
    #undef UART_BLOCK_TICKS
    #define UART_BLOCK_TICKS pdMS_TO_TICKS(1000)
#endif

// ESP NOW Communication ===================================================
#include <WiFi.h>
#include <esp_now.h>

#define ESP_NOW_CHANNEL 1U
#define ESP_NOW_LISTENER_CORE 0

typedef struct 
{
    uint8_t macAddr[6];
    uint8_t data[250];
    int len;
} espNowPacket_t;

QueueHandle_t espNowRecvQueue;

void espNowInit(void);
void espNowDataRecvCallback(const uint8_t *mac_addr, const uint8_t *data, int len);
void espNowDataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status);
void espNowListnener(void *param);
void onESPNowDataReceived(char *cmdText);
void espNowPrintf(const uint8_t *mac_addr, const char *format, ...);
// UWB Communication =======================================================
#include <SPI.h>
#include "dw3000.h"

#define UWB_LISTENER_CORE 0
#define DWT_SFD_IEEE8 0
#define UWB_TIMEOUT
#define TX_ANT_DLY 16385
#define RX_ANT_DLY 16385

// Makersfab ESP32 DW3000 board config
const uint8_t PIN_RST = 27; // reset pin
const uint8_t PIN_IRQ = 34; // irq pin
const uint8_t PIN_SS = 4; // spi select pin

static dwt_config_t uwbConfig = 
{
    5,               /* Channel number. */
    DWT_PLEN_128,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    DWT_PHRRATE_STD, /* PHY header rate. */
    (129 + 8 - 8),   /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF, /* STS disabled */
    DWT_STS_LEN_64,/* STS length see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0      /* PDOA mode off */
};

extern dwt_txconfig_t txconfig_options;

TaskHandle_t uwbTask;

void uwbInit(void);
void _ISR_UWB(void);
// void dwtDataRecvCallback(const dwt_cb_data_t *data);
void uwbListener(void *param);
void onUWBDataReceived(char *cmdText);
void uwbPrintf(uint16_t uwbId, const char *format, ...);
//==========================================================================

void setup()
{
    // Initialize Serial ========================================================
    #ifdef __DEBUG__
    
    Serial.begin(115200);
    while (!Serial) { delay(10); }
    Serial.println("Serial initialized");
    #endif
    //===========================================================================

    // Initialize UART for BH0 Communication ====================================
    uart_init();
    //===========================================================================

    // Initialize ESP-NOW Communication =========================================
    espNowInit();
    delay(500);
    //===========================================================================

    // Initialize UWB Communication ============================================
    uwbInit();
    delay(100);
    //===========================================================================
}

void loop()
{

}

// UART Communication ===========================================================
void uart_init(void)
{
    uart_config_t uartConfig = 
    {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(HHUWB, &uartConfig);
    uart_set_pin(HHUWB, UART1_TX_PIN, UART1_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(HHUWB, UART_RX_BUFFER_SIZE, UART_TX_BUFFER_SIZE, UART_EVENT_QUEUE_SIZE, &hhuwbEventQueue, 0);
    
    delay(500); // delay to ensure Serial is ready before sending data

    xTaskCreatePinnedToCore
    (
        uartListener,
        "uartListener",
        4096,
        NULL,
        1,
        NULL,
        UART_LISTENER_CORE
    );
}

void uartListener(void *param) 
{
    uart_event_t event;
    uint8_t data[MAX_SERIAL_LENGTH];
    
    while(true) 
    {
        if(xQueueReceive(hhuwbEventQueue, (void *)&event, UART_BLOCK_TICKS)) 
        {
            #ifdef __DEBUG__
                pingCount = 0;
                Serial.println("UART RECEIVED");
                isConnected = true;
            #endif
            switch(event.type) 
            {
                case UART_DATA:
                {
                    size_t readLen = min(event.size, sizeof(data) - 1);
                    int len = uart_read_bytes(UART_NUM_1, data, readLen, pdMS_TO_TICKS(100));
                    
                    if(len > 0) 
                    {
                        data[len] = '\0';
                        onUARTDataReceived((char *)data);
                    }
                    break;
                }
                
                case UART_FIFO_OVF:
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(hhuwbEventQueue);
                    break;
                
                default:
                    #ifdef __DEBUG__
                        Serial.printf("Unknown UART Event Type: %d\n", event.type);
                    #endif
                    break;
            }
        }
        else
        {
            #ifdef __DEBUG__
                uart_write_bytes(UART_NUM_1, "PING\n", 5);
                if(pingCount > MAX_PING_COUNT)
                {
                    pingCount = 0;
                    isConnected = false;
                }
                else
                    {pingCount++;}
            #endif
        }
        
    }
}

void onUARTDataReceived(char *cmdText)
{
    if(strcmp(cmdText, "PONG") == 0)
    {
        // Ping Pong Command, Do nothing
    }
    else if(strcmp(cmdText, "") == 0)
    {

    }
    else
    {
        #ifdef __DEBUG__
            Serial.printf("Unknown Command Received : %s\n", cmdText);
        #endif
    }
}

void uartPrintf(uart_port_t uart_num, const char *format, ...)
{
    char buffer[250];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    uart_write_bytes(uart_num, buffer, strlen(buffer));
}

// ESP NOW Communication ========================================================
void espNowInit(void)
{
    WiFi.mode(WIFI_STA);
    if(esp_now_init() != ESP_OK)
    {
        #ifdef __DEBUG__
            Serial.println("ESP-NOW Initialization Failed");
        #endif
        return;
    }

    for(int i = 0 ; i < MAX_BOARDS ; i++)
    {
        esp_now_peer_info_t peerInfo = {};
        peerInfo.channel = ESP_NOW_CHANNEL;
        peerInfo.encrypt = false;
        if(i == CurrentBoard)
        {
            peerInfo.peer_addr[0] = 0x02; // Locally Administered dummy Address
        }
        else
        {
            memcpy(peerInfo.peer_addr, MAC_ADDR[i], 6);
        }
        if(esp_now_add_peer(&peerInfo) != ESP_OK)
        {
            #ifdef __DEBUG__
                Serial.println("Failed to add ESP-NOW peer");
            #endif
        }
        else
        {
            #ifdef __DEBUG__
            Serial.print("Added ESP-NOW peer: ");
            Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n", peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
            #endif
        }
}

    esp_now_register_recv_cb(espNowDataRecvCallback);
    esp_now_register_send_cb(espNowDataSendCallback);
    espNowRecvQueue = xQueueCreate(10, sizeof(espNowPacket_t));
    xTaskCreatePinnedToCore
    (
        espNowListnener,
        "espNowListener",
        4096,
        NULL,
        2,
        NULL,
        ESP_NOW_LISTENER_CORE
    );
}

void espNowDataRecvCallback(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    espNowPacket_t buffer;
    memcpy(buffer.macAddr, mac_addr, 6);
    memcpy(buffer.data, data, len);
    buffer.len = len;
    xQueueSendFromISR(espNowRecvQueue, &buffer, NULL);
}

void espNowListnener(void *param)
{
    espNowPacket_t packet;
    while(true)
    {
        if(xQueueReceive(espNowRecvQueue, &packet, portMAX_DELAY))
        {
            #ifdef __DEBUG__
                Serial.println("ESP-NOW Data Received");
            #endif
            boolean isKnownSender = false;
            for(int i = 0 ; i < MAX_BOARDS ; i++)
            {
                if(memcmp(packet.macAddr, MAC_ADDR[i], 6) == 0)
                {
                    isKnownSender = true;
                    break;
                }
            }

            if(!isKnownSender)
            {
                #ifdef __DEBUG__
                    Serial.println("Unknown ESP-NOW Sender, Ignoring Packet");
                #endif
                continue;
            }
            else
            {
                if(packet.len > 0) 
                {
                    packet.data[packet.len] = '\0';
                    onESPNowDataReceived((char *)packet.data);
                }
                #ifdef __DEBUG__
                    else
                    {
                        Serial.println("Received Empty ESP-NOW Packet");
                    }
                #endif
            }
        }
    }
}

void onESPNowDataReceived(char *cmdText)
{
    if(strcmp(cmdText, "PONG") == 0)
    {
        // Ping Pong Command, Do nothing
    }
    else if(strcmp(cmdText, "") == 0)
    {

    }
    else
    {
        #ifdef __DEBUG__
            Serial.printf("Unknown ESP-NOW Command Received : %s\n", cmdText);
        #endif
    }
}

void espNowDataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    #ifdef __DEBUG__
        Serial.print("ESP-NOW Data Send Callback, Status: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
    #endif
}

void espNowPrintf(const uint8_t *mac_addr, const char *format, ...)
{
    char buffer[250];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    esp_now_send(mac_addr, (uint8_t *)buffer, strlen(buffer));
}

// UWB Communication ===========================================================
void uwbInit(void)
{
    #ifdef __DEBUG__
        Serial.println("UWB Init.");
    #endif

    

    //REFACTOR : all these code below with lower class driver if it needed
    spiBegin(PIN_IRQ, PIN_RST);
    spiSelect(PIN_SS);

    delay(2); // Time needed for DW3000 to start up (transition from INIT_RC to IDLE_RC, or could wait for SPIRDY event)

    while (!dwt_checkidlerc()) // Need to make sure DW IC is in IDLE_RC before proceeding 
    {
        UART_puts("IDLE FAILED\r\n");
        while (1) ;
    }

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
    UART_puts("INIT FAILED\r\n");
    while (1) ;
    }

    // Enabling LEDs here for debug so that for each TX the D1 LED will flash on DW3000 red eval-shield boards.
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure DW IC. See NOTE 6 below. */
    if(dwt_configure(&uwbConfig)) // if the dwt_configure returns DWT_ERROR either the PLL or RX calibration has failed the host should reset the device
    {
    UART_puts("CONFIG FAILED\r\n");
    while (1) ;
    }
    /* Apply default antenna delay value. See NOTE 2 below. */
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);

    /* Next can enable TX/RX states output on GPIOs 5 and 6 to help debug, and also TX/RX LEDs
    * Note, in real low power applications the LEDs should not be used. */
    dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE); 

    pinMode(PIN_IRQ, INPUT);
    attachInterrupt(PIN_IRQ, _ISR_UWB, RISING);
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    dwt_setinterrupt(
        SYS_ENABLE_LO_RXFCG_ENABLE_BIT_MASK |
        SYS_ENABLE_LO_RXFCE_ENABLE_BIT_MASK |
        SYS_ENABLE_LO_RXFSL_ENABLE_BIT_MASK |
        SYS_ENABLE_LO_RXSTO_ENABLE_BIT_MASK |
        SYS_ENABLE_LO_RXFTO_ENABLE_BIT_MASK,
        0,
        DWT_ENABLE_INT
    );
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    delay(500);


    // dwt_setcallbacks(NULL, dwtDataRecvCallback, NULL, NULL, NULL, NULL);

    xTaskCreatePinnedToCore
    (
        uwbListener,
        "uwbListener",
        4096,
        NULL,
        0,
        &uwbTask,
        UWB_LISTENER_CORE
    );
}

void _ISR_UWB(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(uwbTask, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// void dwtDataRecvCallback(const dwt_cb_data_t *data)
// {
//     // #ifdef __DEBUG__
//     //     Serial.println("Callback Call!");
//     // #endif
//     const dwt_cb_data_t buffer = 
//     {
//         data->datalength,
//         data->rx_flags,
//         data->status,
//         data->status_hi
//     };
//     // xQueueSendFromISR(uwbEventQueue, (void *)&buffer, NULL);
// }

void uwbListener(void *param)
{
    // dwt_cb_data_t data;
    // while(true)
    // {
    //     if(xQueueReceive(uwbEventQueue, &data, portMAX_DELAY))
    //     {
    //         #ifdef __DEBUG__
    //             Serial.printf("UWB RX\n");
    //         #endif
    //         dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_GOOD | SYS_STATUS_ALL_RX_ERR | SYS_STATUS_ALL_RX_TO);
    //         dwt_rxenable(DWT_START_RX_IMMEDIATE);

    //     }
    // }
    #ifdef __DEBUG__
        Serial.println("UWBListener Initialized");
    #endif

    while(true)
    {
        if(ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0)
        {
            #ifdef __DEBUG__
                Serial.println("UWB Interrupt!");
            #endif
            dwt_isr();
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
}

void onUWBDataReceived(char *cmdText)
{

}

void uwbPrintf(uint16_t uwbId, const char *format, ...)
{

}
