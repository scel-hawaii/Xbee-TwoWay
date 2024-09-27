#define START_BYTE 0x7E
#define ESCAPE 0x7D
#define XON 0x11
#define XOFF 0x13

const uint8_t coordinatorAddress[] = {0x00, 0x13, 0xA2, 0x00, 0x40, 0x9F, 0x70, 0x67};
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // Send every 10 seconds

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino XBee Router - API Mode 2 (No Library)");
}

void sendMessage(String message) {
  uint8_t payload[message.length() + 1];
  message.getBytes(payload, sizeof(payload));
  
  // Construct API frame
  uint8_t frame[256];
  int frameLength = 14 + message.length(); // Frame type + FrameID + Addr64 + Addr16 + Radius + Options + Data
  frame[0] = START_BYTE;
  frame[1] = (frameLength >> 8) & 0xFF; // Length MSB
  frame[2] = frameLength & 0xFF; // Length LSB
  frame[3] = 0x10; // Frame type (Transmit Request)
  frame[4] = 0x01; // Frame ID
  memcpy(&frame[5], coordinatorAddress, 8); // 64-bit dest address
  frame[13] = 0xFF; // 16-bit dest address (unknown)
  frame[14] = 0xFE;
  frame[15] = 0x00; // Broadcast radius (0 = max)
  frame[16] = 0x00; // Options
  memcpy(&frame[17], payload, message.length());
  
  // Calculate checksum
  uint8_t checksum = 0;
  for (int i = 3; i < 17 + message.length(); i++) {
    checksum += frame[i];
  }
  frame[17 + message.length()] = 0xFF - checksum;
  
  // Send frame with escape characters
  Serial.write(START_BYTE);
  for (int i = 1; i < 18 + message.length(); i++) {
    if (frame[i] == START_BYTE || frame[i] == ESCAPE || frame[i] == XON || frame[i] == XOFF) {
      Serial.write(ESCAPE);
      Serial.write(frame[i] ^ 0x20);
    } else {
      Serial.write(frame[i]);
    }
  }
  
  Serial.println("Sent to Coordinator: " + message);
}

void receiveMessage() {
  static bool frameStarted = false;
  static bool escaped = false;
  static uint8_t frameData[256];
  static int framePos = 0;
  static int frameLength = 0;

  while (Serial.available() > 0) {
    uint8_t byteRead = Serial.read();

    // Handle escape character
    if (escaped) {
      byteRead ^= 0x20;
      escaped = false;
    } else if (byteRead == ESCAPE) {
      escaped = true;
      continue;
    }

    // Start of frame
    if (!frameStarted) {
      if (byteRead == START_BYTE) {
        frameStarted = true;
        framePos = 0;
        frameLength = 0;
        Serial.println("Frame start detected");
      }
      continue;
    }

    // Read length (2 bytes)
    if (framePos < 2) {
      frameData[framePos++] = byteRead;
      if (framePos == 2) {
        frameLength = (frameData[0] << 8) | frameData[1];
        Serial.print("Frame length: ");
        Serial.println(frameLength);
        if (frameLength > sizeof(frameData) - 2) {
          Serial.println("Frame too long, resetting");
          frameStarted = false;
          framePos = 0;
          continue;
        }
      }
      continue;
    }

    // Read frame data
    frameData[framePos++] = byteRead;

    // Check if frame is complete
    if (framePos == frameLength + 2) {
      // Calculate checksum
      uint8_t checksum = 0;
      for (int i = 2; i < framePos - 1; i++) {
        checksum += frameData[i];
      }
      checksum = 0xFF - checksum;

      if (checksum != frameData[framePos - 1]) {
        Serial.print("Checksum error. Calculated: 0x");
        Serial.print(checksum, HEX);
        Serial.print(", Received: 0x");
        Serial.println(frameData[framePos - 1], HEX);
        // Print the entire frame for debugging
        Serial.print("Raw frame: ");
        for (int i = 0; i < framePos; i++) {
          Serial.print(frameData[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
      } else {
        Serial.println("Checksum OK");
      }

      uint8_t apiIdentifier = frameData[2];
      if (apiIdentifier == 0x90) { // ZigBee Receive Packet
        // Extract payload
        int dataStart = 14; // Corrected start index for RF data
        int dataLength = framePos - 1 - dataStart; // Exclude checksum
        String receivedMessage = "";
        for (int i = dataStart; i < framePos - 1; i++) {
          receivedMessage += (char)frameData[i];
        }
        Serial.println("Received from Coordinator: " + receivedMessage);
      } else {
        Serial.print("Received unhandled frame type: 0x");
        Serial.println(apiIdentifier, HEX);
      }

      // Reset for next frame
      frameStarted = false;
      framePos = 0;
    }
  }
}


void loop() {
  unsigned long currentMillis = millis();

  // Send message to Coordinator periodically
  if (currentMillis - lastSendTime >= sendInterval) {
    String message = "Hello from Router!";
    sendMessage(message);
    lastSendTime = currentMillis;
  }

  // Continuously check for incoming messages from Coordinator
  receiveMessage();
}
