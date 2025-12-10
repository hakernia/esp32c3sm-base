#include <esp_now.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "secrets_local.h"

// MAC Address of the Server ESP32-C3 (Replace with your server's MAC)
uint8_t serverMacAddress[] = SERVER_MAC_ADDRESS; 

// Define button pins - Use GPIOs that support Light Sleep wakeup
#define BUTTON_1_PIN GPIO_NUM_1 // Assuming GPIO 1
#define BUTTON_2_PIN GPIO_NUM_2 // Assuming GPIO 2
#define BUTTON_3_PIN GPIO_NUM_3 // Assuming GPIO 3
#define BUTTON_4_PIN GPIO_NUM_4 // Assuming GPIO 4

// The maximum time (in microseconds) to stay in Light Sleep before waking up
#define SLEEP_TIMEOUT_US 30000000ULL // 30 seconds

// Structure for the data to be sent (Must match the Server structure)
typedef struct struct_message {
    int pressedButtons; // Bitmask for pressed buttons
    char messageType[10]; // "data" or "command"
} struct_message;

// Structure for the data to be received (Must match the Server structure)
typedef struct struct_command {
    char command[20]; // e.g., "SLEEP"
} struct_command;

struct_message clientTxData;
struct_command clientRxData;

// A flag to keep the client awake until the server command is received
bool stayActive = true;

// Function to print the wake-up reason
void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    Serial.print("Wakeup cause: ");
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("Timer");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            Serial.println("GPIO");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            Serial.println("Undefined (first boot/reset)");
            break;
        default:
            Serial.printf("Other cause (%d)\n", wakeup_reason);
            break;
    }

    if (wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
        // Read the level of the wakeup pin(s).
        uint64_t wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
        Serial.printf("Wakeup GPIO Mask: 0x%llX\n", wakeup_pin_mask);
    }
}

// Callback when data is sent
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    const uint8_t *mac_addr = tx_info->des_addr; // Extract MAC address from new struct
    
    Serial.print("\nLast Packet Send Status to: ");
    for(int i=0; i<6; i++) {
        Serial.printf("%02X", mac_addr[i]);
        if(i<5) Serial.print(":");
    }
    Serial.print(" | Status: ");

    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("Delivery Success");
    } else {
        Serial.println("Delivery Fail");
    }
}

// Callback when data is received
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
    // const uint8_t *mac = recv_info->peer_addr; // MAC address is in the struct
    
    // We only care about the incoming data for the command
    memcpy(&clientRxData, incomingData, sizeof(clientRxData));
    Serial.printf("\nMessage from Server: %s\n", clientRxData.command);

    if (strcmp(clientRxData.command, "SLEEP") == 0) {
        // Received the command to go to sleep
        Serial.println("Received 'SLEEP' command. Preparing for Light Sleep...");
        stayActive = false;
    }
}

#define DEBOUNCE_N  5
uint16_t debounceBits(uint16_t inputBits) {
    static uint8_t count[16] = {0};
    const uint8_t MAX = DEBOUNCE_N * 2;
    uint16_t output = 0;
    for (uint8_t bit = 0; bit < 16; bit++) {
        uint16_t mask = 1 << bit;

        if (inputBits & mask) {
            if (count[bit] < MAX) count[bit]++;
            if (count[bit] == DEBOUNCE_N) count[bit] = MAX;
        } else {
            if (count[bit] > 0) count[bit]--;
            if (count[bit] == DEBOUNCE_N) count[bit] = 0;
        }
        if (count[bit] >= DEBOUNCE_N)
            output |= mask;
    }
    return output;
}


// Function to check button states and set the message payload
bool check_buttons() {
    static int previousButtons;
    clientTxData.pressedButtons = 0;
    
    // Check buttons (LOW when pressed due to pull-up)
    if (digitalRead(BUTTON_1_PIN) == LOW) clientTxData.pressedButtons |= (1 << 0);
    if (digitalRead(BUTTON_2_PIN) == LOW) clientTxData.pressedButtons |= (1 << 1);
    if (digitalRead(BUTTON_3_PIN) == LOW) clientTxData.pressedButtons |= (1 << 2);
    if (digitalRead(BUTTON_4_PIN) == LOW) clientTxData.pressedButtons |= (1 << 3);

    clientTxData.pressedButtons = debounceBits(clientTxData.pressedButtons);
    if(clientTxData.pressedButtons != previousButtons) {
        previousButtons = clientTxData.pressedButtons;
        return true;  // zmiana
    } else {
        return false; // brak zmian
    }
}

void setup() {
    Serial.begin(115200);
    stayActive = true; // initially active, server may request sleep

    // Initial setup for the button pins
    pinMode(BUTTON_1_PIN, INPUT_PULLUP);
    pinMode(BUTTON_2_PIN, INPUT_PULLUP);
    pinMode(BUTTON_3_PIN, INPUT_PULLUP);
    pinMode(BUTTON_4_PIN, INPUT_PULLUP);

    // Print wake-up reason for debugging
    print_wakeup_reason();

    // Configure Wi-Fi in Station mode
    WiFi.mode(WIFI_STA);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    Serial.print("Client MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Register peer (Server)
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, serverMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(serverMacAddress)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to add peer");
            return;
        }
    }

    // Register send and receive callbacks
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    // Check button status and prepare the message on wake-up
    check_buttons();

    Serial.println("Client is active, waiting for 'SLEEP' command from server...");
}

void loop() {
    // This loop runs when the client is in the active state (stayActive = true)
    if (stayActive) {

        if (check_buttons()) {
            strcpy(clientTxData.messageType, "data");
            Serial.printf("Buttons: %d. Sending message...\n", clientTxData.pressedButtons);
            esp_err_t result = esp_now_send(serverMacAddress, (uint8_t *) &clientTxData, sizeof(clientTxData));
            if (result != ESP_OK) {
                Serial.println("Error sending the data");
            } else {
                Serial.println("Data queued for sending.");
            }
        }

    } else {
        enter_light_sleep();
    }
}

// Function to configure and enter light sleep
void enter_light_sleep() {
    Serial.println("--- Entering Light Sleep Mode ---");

    // 1. Disable Wi-Fi/BT before sleep
    // Wi-Fi.mode(WIFI_OFF) is generally sufficient for ESP-NOW/Light Sleep
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 2. Configure Wake-up Sources
    
    // Enable Timer Wake-up
    // esp_sleep_enable_timer_wakeup(SLEEP_TIMEOUT_US);
    // Serial.printf("Timer wakeup set for %llu uS.\n", SLEEP_TIMEOUT_US);

    // Enable GPIO Wake-up for all 4 buttons (LOW level on press)
    // Note: The ESP32-C3 in Light Sleep can use gpio_wakeup_enable() for any GPIO.
    gpio_wakeup_enable(BUTTON_1_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(BUTTON_2_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(BUTTON_3_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(BUTTON_4_PIN, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    Serial.println("GPIO wakeup enabled on multiple pins (LOW level).");

    // 3. Enter Light Sleep
    // The program will pause here. It will resume execution right after this call 
    // when a wake-up source is triggered.
    esp_light_sleep_start();

    // --- Woken Up ---
    Serial.println("--- Woken up! ---");
    
    // Re-initialize to active state logic
    delay(100);
    // Restart active state flag check
    stayActive = false;
    // The setup() function handles the rest after wake-up (re-init Wi-Fi, check buttons, send data).
    // In Arduino, when waking from Light Sleep, the code continues from esp_light_sleep_start().
    // We can jump back to setup() or handle re-initialization here.
    
    // We *must* re-initialize Wi-Fi/ESP-NOW/Serial to be able to communicate again.
    
/*
    // Re-initialize WiFi/ESP-NOW environment (it was turned off before sleep)
    WiFi.mode(WIFI_STA);
    esp_now_init();
    
    // Re-register callbacks and peer (as re-init can clear them)
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);
    
    // Re-add peer (check first to avoid error)
    if (!esp_now_is_peer_exist(serverMacAddress)) {
        esp_now_peer_info_t peerInfo;
        memcpy(peerInfo.peer_addr, serverMacAddress, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }
*/

    // Now, run the setup logic again to check the wake-up reason and send a message.
    setup(); 
}