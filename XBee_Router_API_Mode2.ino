#include <XBee.h>
#include <SoftwareSerial.h>

XBee xbee = XBee();
XBeeAddress64 coordinatorAddress = XBeeAddress64(0x0013A200, 0x409F7067);
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // Send every 1 second
bool isDataSendingActive = false;
bool contactEstablished = false;
const unsigned long initialContactInterval = 5000; // Try initial contact every 5 seconds
unsigned long lastInitialContactAttempt = 0;

unsigned long lastStatusPrint = 0;
const unsigned long statusPrintInterval = 5000;  // Print status every 5 seconds

// Use pins 2 and 3 for software serial debugging
SoftwareSerial debugSerial(2, 3); // RX, TX

void setup() {
  Serial.begin(9600);  // XBee communication
  debugSerial.begin(9600);  // Debug output
  delay(1000);  // Add a 1-second delay for XBee initialization
  xbee.setSerial(Serial);
  
  debugSerial.println("Arduino XBee Router - Enhanced Debugging");
  debugSerial.println("Coordinator Address: 0x0013A200 0x409F7067");
}

bool sendMessage(String message) {
  uint8_t payload[message.length() + 1];
  message.toCharArray((char*)payload, sizeof(payload));
  
  ZBTxRequest zbTx = ZBTxRequest(coordinatorAddress, payload, sizeof(payload) - 1);
  xbee.send(zbTx);
  
  debugSerial.print("Sending to Coordinator: ");
  debugSerial.println(message);
  debugSerial.print("Payload size: ");
  debugSerial.println(sizeof(payload) - 1);
  
  // Wait for and parse the TX status response
  if (xbee.readPacket(5000)) {
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      ZBTxStatusResponse txStatus;
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        debugSerial.println("Message sent successfully!");
        return true;
      } else {
        debugSerial.print("Message send failed. Status code: 0x");
        debugSerial.println(txStatus.getDeliveryStatus(), HEX);
        return false;
      }
    }
  } else if (xbee.getResponse().isError()) {
    debugSerial.print("Error reading packet. Error code: ");
    debugSerial.println(xbee.getResponse().getErrorCode());
    return false;
  } else {
    debugSerial.println("No response received");
    return false;
  }
}

void receiveMessage() {
  xbee.readPacket(100);  // Read for 100ms
  
  if (xbee.getResponse().isAvailable()) {
    uint8_t apiId = xbee.getResponse().getApiId();
    debugSerial.print("Received API frame. API ID: 0x");
    debugSerial.println(apiId, HEX);
    
    if (apiId == ZB_RX_RESPONSE) {
      ZBRxResponse rx;
      xbee.getResponse().getZBRxResponse(rx);
      
      uint8_t* data = rx.getData();
      uint8_t dataLength = rx.getDataLength();
      char message[dataLength + 1];
      memcpy(message, data, dataLength);
      message[dataLength] = '\0';
      String messageStr = String(message);
      
      debugSerial.print("Received from Coordinator: ");
      debugSerial.println(messageStr);

      if (messageStr == "Contact confirmed") {
        contactEstablished = true;
        debugSerial.println("Contact established. Waiting for start command.");
      } else if (messageStr == "pause") {
        isDataSendingActive = false;
        debugSerial.println("Data sending paused");
      } else if (messageStr == "start") {
        isDataSendingActive = true;
        debugSerial.println("Data sending started");
      } else if (messageStr == "heartbeat") {
        debugSerial.println("Heartbeat received");
        sendMessage("heartbeat_ack");  // Acknowledge the heartbeat
      }
      
      debugSerial.print("Current status - Contact Established: ");
      debugSerial.print(contactEstablished);
      debugSerial.print(", Data Sending Active: ");
      debugSerial.println(isDataSendingActive);
    }
    else {
      debugSerial.print("Unhandled API ID: 0x");
      debugSerial.println(apiId, HEX);
    }
  } else if (xbee.getResponse().isError()) {
    debugSerial.print("Error reading packet. Error code: ");
    debugSerial.println(xbee.getResponse().getErrorCode());
  }
}

void attemptInitialContact() {
  if (!contactEstablished && millis() - lastInitialContactAttempt >= initialContactInterval) {
    debugSerial.println("Attempting initial contact...");
    if (sendMessage("Initial contact")) {
      debugSerial.println("Initial contact message sent. Waiting for confirmation...");
    } else {
      debugSerial.println("Failed to send initial contact message. Will retry.");
    }
    lastInitialContactAttempt = millis();
  }
}

void printStatus() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastStatusPrint >= statusPrintInterval) {
    debugSerial.println("--- Current Status ---");
    debugSerial.print("Contact Established: ");
    debugSerial.println(contactEstablished ? "Yes" : "No");
    debugSerial.print("Data Sending Active: ");
    debugSerial.println(isDataSendingActive ? "Yes" : "No");
    debugSerial.println("----------------------");
    lastStatusPrint = currentMillis;
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (!contactEstablished) {
    attemptInitialContact();
  } 
  
  if (isDataSendingActive && contactEstablished) {
    if (currentMillis - lastSendTime >= sendInterval) {
      debugSerial.println("Attempting to send data...");
      sendMessage("data");
      lastSendTime = currentMillis;
    }
  }

  receiveMessage();
  printStatus();
  
  delay(10);  // Small delay to prevent flooding the serial port
}
