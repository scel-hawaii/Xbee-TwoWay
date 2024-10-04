#include <XBee.h>

XBee xbee = XBee();
XBeeAddress64 coordinatorAddress = XBeeAddress64(0x0013A200, 0x409F7067);
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // Send every 1 second
bool isDataSendingActive = false;
bool contactEstablished = false;
const unsigned long initialContactInterval = 5000; // Try initial contact every 5 seconds
unsigned long lastInitialContactAttempt = 0;
const int maxRetries = 3;

unsigned long lastStatusPrint = 0;
const unsigned long statusPrintInterval = 5000;  // Print status every 5 seconds

void setup() {
  Serial.begin(9600);
  delay(1000);  // Add a 1-second delay for XBee initialization
  xbee.setSerial(Serial);
  
  Serial.println("Arduino XBee Router - Enhanced Debugging");
  Serial.println("Coordinator Address: 0x0013A200 0x409F7067");
}

bool sendMessage(String message, int retries = 0) {
  uint8_t payload[message.length() + 1];
  message.toCharArray((char*)payload, sizeof(payload));
  
  ZBTxRequest zbTx = ZBTxRequest(coordinatorAddress, payload, sizeof(payload) - 1);
  xbee.send(zbTx);
  
  Serial.print("Sending to Coordinator: ");
  Serial.println(message);
  Serial.print("Payload size: ");
  Serial.println(sizeof(payload) - 1);
  
  // Get the status of the transmission
  ZBTxStatusResponse txStatus;
  if (xbee.readPacket(10000)) { // Increased timeout to 10 seconds
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        Serial.println("Message sent successfully!");
        return true;
      } else {
        Serial.print("Message send failed. Status code: 0x");
        Serial.println(txStatus.getDeliveryStatus(), HEX);
        
        if (retries < maxRetries) {
          Serial.println("Retrying...");
          delay(1000); // Wait a second before retrying
          return sendMessage(message, retries + 1);
        }
      }
    } else {
      Serial.print("Unexpected API frame received. API ID: 0x");
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  } else {
    Serial.println("No transmission status received. XBee might not be in API mode.");
  }
  return false;
}

void receiveMessage() {
  xbee.readPacket(100);  // Read for 100ms
  
  if (xbee.getResponse().isAvailable()) {
    Serial.print("Received API frame. API ID: 0x");
    Serial.println(xbee.getResponse().getApiId(), HEX);
    
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      ZBRxResponse rx;
      xbee.getResponse().getZBRxResponse(rx);
      
      uint8_t* data = rx.getData();
      uint8_t dataLength = rx.getDataLength();
      char message[dataLength + 1];
      memcpy(message, data, dataLength);
      message[dataLength] = '\0';
      String messageStr = String(message);
      
      Serial.print("Received from Coordinator: ");
      Serial.println(messageStr);

      if (messageStr == "Contact confirmed") {
        contactEstablished = true;
        Serial.println("Contact established. Waiting for start command.");
      } else if (messageStr == "pause") {
        isDataSendingActive = false;
        Serial.println("Data sending paused");
      } else if (messageStr == "start") {
        isDataSendingActive = true;
        Serial.println("Data sending started");
      } else if (messageStr == "heartbeat") {
        Serial.println("Heartbeat received");
        sendMessage("heartbeat_ack");  // Acknowledge the heartbeat
      }
      
      Serial.print("Current status - Contact Established: ");
      Serial.print(contactEstablished);
      Serial.print(", Data Sending Active: ");
      Serial.println(isDataSendingActive);
    } else {
      Serial.print("Unexpected API frame received. API ID: 0x");
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  }
}

void attemptInitialContact() {
  if (!contactEstablished && millis() - lastInitialContactAttempt >= initialContactInterval) {
    Serial.println("Attempting initial contact...");
    if (sendMessage("Initial contact")) {
      Serial.println("Initial contact message sent successfully. Waiting for confirmation...");
    } else {
      Serial.println("Failed to send initial contact after multiple retries. Will try again later.");
    }
    lastInitialContactAttempt = millis();
  }
}

void printStatus() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusPrint >= statusPrintInterval) {
    Serial.println("--- Current Status ---");
    Serial.print("Contact Established: ");
    Serial.println(contactEstablished ? "Yes" : "No");
    Serial.print("Data Sending Active: ");
    Serial.println(isDataSendingActive ? "Yes" : "No");
    Serial.println("----------------------");
    lastStatusPrint = currentMillis;
  }
}

void loop() {
  if (!contactEstablished) {
    attemptInitialContact();
  } else if (isDataSendingActive) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= sendInterval) {
      Serial.println("Attempting to send data...");
      if (sendMessage("data")) {
        lastSendTime = currentMillis;
        Serial.println("Data sent successfully");
      } else {
        Serial.println("Failed to send data. Will retry on next interval.");
      }
    }
  } else {
    Serial.println("Contact established but data sending is not active. Waiting for start command.");
    delay(1000);  // Wait for 1 second before checking again
  }
  receiveMessage();
  printStatus();
  delay(10);  // Add a small delay to prevent flooding the serial port
}
