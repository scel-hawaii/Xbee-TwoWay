#include <XBee.h>

XBee xbee = XBee();
XBeeAddress64 coordinatorAddress = XBeeAddress64(0x0013A200, 0x409F7067);

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // Send every 10 seconds

void setup() {
  Serial.begin(9600);
  xbee.begin(Serial);
  
  Serial.println("Arduino XBee Router - Using XBee.h library");
  Serial.println("Coordinator Address: 0x0013A200 0x409F7067");
}

void sendMessage(String message) {
  uint8_t payload[message.length() + 1];
  message.toCharArray((char*)payload, sizeof(payload));
  
  ZBTxRequest zbTx = ZBTxRequest(coordinatorAddress, payload, sizeof(payload) - 1);
  xbee.send(zbTx);
  
  Serial.print("Sending to Coordinator: ");
  Serial.println(message);
  
  // Get the status of the transmission
  ZBTxStatusResponse txStatus;
  if (xbee.readPacket(5000)) {
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        Serial.println("Message sent successfully!");
      } else {
        Serial.print("Message send failed. Status code: ");
        Serial.println(txStatus.getDeliveryStatus(), HEX);
      }
    }
  } else {
    Serial.println("No transmission status received.");
  }
}

void receiveMessage() {
  xbee.readPacket();
  
  if (xbee.getResponse().isAvailable()) {
    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      ZBRxResponse rx;
      xbee.getResponse().getZBRxResponse(rx);
      
      uint8_t* data = rx.getData();
      String message = String((char*)data);
      
      Serial.print("Received from Coordinator: ");
      Serial.println(message);
    } else {
      Serial.print("Received unexpected API frame. API ID: ");
      Serial.println(xbee.getResponse().getApiId(), HEX);
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSendTime >= sendInterval) {
    sendMessage("Hello from Router!");
    lastSendTime = currentMillis;
  }

  receiveMessage();
}
