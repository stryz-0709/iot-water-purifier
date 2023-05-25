#include <Adafruit_Fingerprint.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Wi-Fi credentials
const char* ssid = "Duy Minh";
const char* password = "SquareDogs";

// MQTT broker settings
const char* mqttServer = "rabbitmq-pub.education.wise-paas.com";
const int mqttPort = 1883;
const char* mqttUsername = "EoqrxoPnEPQv:8xQnpnbJOMqD";
const char* mqttPassword = "4duJ8w9xlyD8xDemE9uN";
const char* mqttIdTopic = "water/id";
const char* mqttDataTopic = "water/data";
const char* mqttFingerprintTopic = "fingerprint/type";

// Adafruit fingerprint sensor
SoftwareSerial fingerprintSerial(D5, D6); // Assuming the fingerprint sensor is connected to digital pins D1 and D2
Adafruit_Fingerprint fingerprint = Adafruit_Fingerprint(&fingerprintSerial);

// MQTT client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Water data
int waterValue = 0;
String waterQuality = "";
bool fingerprintEnrollRequest = false;
int fingerprintId = 0;

void setup() {
  Serial.begin(9600);
  mqttClient.setCallback(callback);
  delay(1000);
  fingerprintId = 0;

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Connected to Wi-Fi");

  // Connect to MQTT broker
  mqttClient.setServer(mqttServer, mqttPort);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (mqttClient.connect("ESP8266Client", mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }

  // Subscribe to MQTT topic
  mqttClient.subscribe(mqttFingerprintTopic);

  // Initialize fingerprint sensor
  fingerprintSerial.begin(57600);
  if (!fingerprint.verifyPassword()) {
    Serial.println("Failed to initialize fingerprint sensor");
    while (1);
  }

  Serial.println("Fingerprint enrollment ready");
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }

  mqttClient.loop();

  if (fingerprintEnrollRequest) {
    if (enrollFingerprint()) {
      Serial.print("Fingerprint enrolled with ID: ");
      Serial.println(fingerprintId);

      // Publish the fingerprint ID
      String idPayload = String(fingerprintId);
      mqttClient.publish(mqttIdTopic, idPayload.c_str());

      delay(500);  // Wait for a moment before scanning the next fingerprint
    } else {
      Serial.println("Failed to enroll fingerprint. Please try again.");
    }
    fingerprintEnrollRequest = false;
  }

  // Generate random water value and quality
  waterValue = random(0, 100);
  waterQuality = random(0, 2) == 0 ? "Good" : "Poor";

  // Publish water data
  publishWaterData();

  delay(5000);  // Wait for 5 seconds before generating next water data
}

void publishWaterData() {
  // Create JSON payload for water data
  DynamicJsonDocument doc(128);
  doc["id"] = fingerprintId;
  doc["value"] = waterValue;
  doc["quality"] = waterQuality;

  // Convert JSON document to a string
  String payload;
  serializeJson(doc, payload);

  // Publish water data
  mqttClient.publish(mqttDataTopic, payload.c_str());
}

bool enrollFingerprint() {
  Serial.println("Place your finger on the fingerprint sensor...");
  int result = -1;
  while (result != FINGERPRINT_OK){
    result = fingerprint.getImage();
    switch (result) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  result = fingerprint.image2Tz(1);
  switch (result) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return false;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return false;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return false;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      return false;
    default:
      Serial.println("Unknown error");
      return false;
  }

  Serial.println("Remove your finger from the fingerprint sensor...");

  delay(2000);
  result = 0;
  while (result != FINGERPRINT_NOFINGER) {
    result = fingerprint.getImage();
  }
  Serial.println("Place your finger on the fingerprint sensor again...");
  result = -1;
  while (result != FINGERPRINT_OK){
    result = fingerprint.getImage();
    switch (result) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  result = fingerprint.image2Tz(2);
  switch (result) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return false;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return false;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return false;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Invalid image");
      return false;
    default:
      Serial.println("Unknown error");
      return false;
  }

  result = fingerprint.createModel();
  if (result == FINGERPRINT_OK) {
    result = fingerprint.storeModel(fingerprintId);
    if (result == FINGERPRINT_OK) {
      return true;
    } else {
      Serial.println("Failed to store fingerprint template");
      return false;
    }
  } else {
    Serial.println("Failed to create fingerprint model");
    return false;
  }
  return true;
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (mqttClient.connect("ESP8266Client", mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

int scanFingerprint() {
  Serial.println("Place your finger on the fingerprint sensor...");
  int result = -1;
  while (result != FINGERPRINT_OK) {
    result = fingerprint.getImage();
    switch (result) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  result = fingerprint.image2Tz();
  if (result != FINGERPRINT_OK) {
    Serial.println("Failed to convert fingerprint image");
    return -1;
  }

  result = fingerprint.fingerSearch();
  if (result == FINGERPRINT_OK) {
    int fingerprintId = fingerprint.fingerID;
    return fingerprintId;
  } else {
    Serial.println("Fingerprint not found");
    return -1;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String receivedTopic = String(topic);
  String receivedPayload = "";

  for (unsigned int i = 0; i < length; i++) {
    receivedPayload += (char)payload[i];
  }

  Serial.print("Received message on topic: ");
  Serial.println(receivedTopic);
  Serial.print("Payload: ");
  Serial.println(receivedPayload);

  if (receivedTopic.equals("fingerprint/type")) {
    // The received message is from the "fingerprint/type" topic
    // Add your code here to handle the message
    // For example, check the payload value or perform fingerprint scanning
    if (receivedPayload.equals("0")) {
      // Fingerprint scanning request
      fingerprintId = scanFingerprint();

      if (fingerprintId != -1) {
        // Fingerprint scanned successfully
        Serial.print("Fingerprint scanned with ID: ");
        Serial.println(fingerprintId);

        // Publish the fingerprint ID
        String idPayload = String(fingerprintId);
        mqttClient.publish(mqttIdTopic, idPayload.c_str());
      } else {
        // Fingerprint scanning failed
        Serial.println("Failed to scan fingerprint. Please try again.");
      }
    }
    else {
      Serial.println("Fingerprint enroll request");
      fingerprintId = receivedPayload.toInt();
      fingerprintEnrollRequest = true;
    }
  }
}



