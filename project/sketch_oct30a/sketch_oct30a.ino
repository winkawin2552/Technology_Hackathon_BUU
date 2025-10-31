/*
  ESP32 Sensor (DHT22, Vibration) to MQTT Publisher
  This code reads sensor data and sends it as a JSON string
  to an MQTT broker.
*/

// --- Include Libraries ---
#include <WiFi.h>
#include <PubSubClient.h> // You must install this from the Library Manager
#include "DHT.h"

// --- WiFi & MQTT Credentials (FILL THESE IN) ---
const char* ssid = "winkawin2552";
const char* password = "winkawin2552";
const char* mqtt_server = "10.50.102.43"; // e.g., "192.168.1.100"
const int mqtt_port = 1883;
// const char* mqtt_user = "your_mqtt_username"; // Uncomment if you set a username
// const char* mqtt_password = "your_mqtt_password"; // Uncomment if you set a password

// --- MQTT Topic ---
const char* out_topic = "esp32/sensors"; // Topic to publish data to

// --- Sensor Pins ---
#define DHTPIN 4
#define VIPIN 34     // GPIO 34 is A6
#define DHTTYPE DHT22

// --- Global Objects ---
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// --- Global Timers ---
unsigned long lastMsg = 0;
// --- FIX 1: Changed from 10 to 2000 to prevent network flooding ---
long msgInterval = 500; // Send data every 2 seconds
char jsonBuffer[256];    // Buffer to hold the JSON string

// --- Function to connect to WiFi ---
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// --- MQTT Callback Function (handles incoming messages) ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// --- MQTT Reconnect Function ---
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "ESP32_Sensor_Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
    // if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// --- Setup Function (runs once) ---
void setup() {
  Serial.begin(9600);
  Serial.println(F("DHTxx test with MQTT!"));
  
  pinMode(VIPIN, INPUT); 
  // --- FIX 2: Changed OUTPUT to INPUT. You are reading from the pin. ---
  dht.begin();

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// --- Main Loop (runs continuously) ---
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  unsigned long now = millis();
  if (now - lastMsg > msgInterval) {
    lastMsg = now;

    // --- Read all sensors ---
    int vibrate = analogRead(VIPIN); 
    float t = dht.readTemperature(); // Read temperature in Celsius

    // --- FIX 3: Simplified the check. You only read 't', so only check 't'. ---
    if (isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return; 
    }

    // --- Format data as JSON (This part was already perfect!) ---
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{\"vibration\":%d, \"temp_c\":%.2f}",
      vibrate, t
    );

    // --- Publish to MQTT ---
    Serial.print("Publishing message: ");
    Serial.println(jsonBuffer);
    client.publish(out_topic, jsonBuffer);
  }
}