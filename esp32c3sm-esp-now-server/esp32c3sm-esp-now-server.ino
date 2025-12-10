#include <esp_now.h>
#include <WiFi.h>
#include "secrets_local.h"

// MAC Address of the Client ESP32-C3 (Replace with your client's MAC)
// Note: This must match the MAC address logged in the Client's Serial Monitor.
uint8_t clientMacAddress[] = CLIENT_MAC_ADDRESS; 

// Structure for the data to be received (Must match the Client structure)
typedef struct struct_message {
    int pressedButtons; // Bitmask for pressed buttons
    char messageType[10]; // "data" or "command"
} struct_message;

// Structure for the data to be sent (Must match the Client structure)
typedef struct struct_command {
    char command[20]; // e.g., "SLEEP"
} struct_command;

struct_message serverRxData;
struct_command serverTxData;

// Corrected Callback when data is received
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
    const uint8_t *mac = recv_info->src_addr; // Extract MAC address from new struct
    
    // ... rest of the logic is the same ...
    memcpy(&serverRxData, incomingData, sizeof(serverRxData));
    
    Serial.println("\n--- Message Received from Client ---");
    Serial.printf("Client MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("Message Type: %s\n", serverRxData.messageType);
    Serial.printf("Pressed Buttons: %d\n", serverRxData.pressedButtons);

    // ... rest of the logic to process data and send command ...
}

// Callback when data is sent (optional for server)
// Corrected Callback when data is sent (optional for server)
void OnDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    // const uint8_t *mac_addr = tx_info->peer_addr; // MAC address is in the struct
    // This is optional for the server, but good for debugging.
    Serial.print("Last Command Send Status: ");
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("Delivery Success");
    } else {
        Serial.println("Delivery Fail");
    }
}

void setup() {
    Serial.begin(115200);

    // Configure Wi-Fi in Station mode
    WiFi.mode(WIFI_STA);

    // Print Server MAC for client configuration
    Serial.print("Server MAC Address: ");
    Serial.println(WiFi.macAddress());

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Register peer (Client)
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, clientMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(clientMacAddress)) {
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to add client peer");
            return;
        }
    }
    
    // Register send and receive callbacks
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("ESP-NOW Server ready. Waiting for messages...");
}

void loop() {
    // The server is in an active state, processing messages in the callback.
    // The main loop can be used for other tasks or simply an indefinite delay.
    delay(10); 
}