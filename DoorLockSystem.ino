// Library
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>

// Defining pin
#define SS_PIN    5   // ESP32 pin GPIO5 
#define RST_PIN   27  // ESP32 pin GPIO27
#define RELAY_PIN 32  // ESP32 pin GPIO32 connects to relay

// Global Variable
bool doorUnlocked = false;
unsigned long unlockTime = 0;

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// UID Key
byte keyTagUID[4] = {0xF1, 0xB2, 0xAF, 0x7B};
byte TestTagUID[4];

// WiFi and MQTT Config
const char *ssidList[2] = {"Helheim"};
const char *passList[2] = {"Nastrand"};
const char *mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *inTopic = "device/roomA";

WiFiClient espClient;
PubSubClient client(espClient);

void displayMessage(const char* line1, const char* line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

bool connectWiFi() {
  for (int i = 0; i < 2; i++) {
    WiFi.begin(ssidList[i], passList[i]);
    for (int attempts = 0; attempts < 10; ++attempts) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected to %s\n", ssidList[i]);
        displayMessage("Connected to", ssidList[i]);
        return true;
      }
      delay(500);
      Serial.print(".");
    }
  }
  Serial.println("\nFailed to connect to WiFi.");
  displayMessage("WiFi connection", "failed");
  return false;
}

void reconnect() {
  displayMessage("Connecting to", "Server");
  while (!client.connected()) {
    Serial.printf("\nConnecting to %s\n", mqtt_broker);
    if (client.connect("Glass")) {
      Serial.println("Connected to MQTT broker");
      client.subscribe(inTopic);
    } else {
      Serial.print("MQTT connection failed, state: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  displayMessage("Scan your ID to", "Access the room");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Message arrived in topic: %s\n", topic);

  // Copy payload to parse hex values
  char payloadStr[length + 1];
  memcpy(payloadStr, payload, length);
  payloadStr[length] = '\0';

  for (int i = 0; i < 4; i++) {
    char hex[3] = {payloadStr[i * 3], payloadStr[i * 3 + 1], '\0'};
    TestTagUID[i] = strtol(hex, nullptr, 16);
  }

  Serial.print("Received UID: ");
  for (byte uid : TestTagUID) Serial.printf("0x%02X ", uid);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  connectWiFi();
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!client.connected()) reconnect();
  client.loop();


  if (doorUnlocked && (millis() - unlockTime >= 3000)) {
    digitalWrite(RELAY_PIN, LOW); // Lock the door
    doorUnlocked = false;
    displayMessage("Scan your ID to", "Access the room");
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (memcmp(rfid.uid.uidByte, keyTagUID, 4) == 0 || memcmp(rfid.uid.uidByte, TestTagUID, 4) == 0) {
      grantAccess("Access granted");
    } else {
      denyAccess();
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void grantAccess(const char* message) {
  Serial.println(message);
  displayMessage(message);
  digitalWrite(RELAY_PIN, HIGH);  // Unlock the door
  doorUnlocked = true;
  unlockTime = millis();
}

void denyAccess() {
  Serial.print("Access denied, UID:");
  for (int i = 0; i < rfid.uid.size; i++) {
    Serial.printf(" 0x%02X", rfid.uid.uidByte[i]);
  }
  Serial.println();
  displayMessage("Access denied");
  delay(3000);
  displayMessage("Scan your ID to", "Access the room");
}
