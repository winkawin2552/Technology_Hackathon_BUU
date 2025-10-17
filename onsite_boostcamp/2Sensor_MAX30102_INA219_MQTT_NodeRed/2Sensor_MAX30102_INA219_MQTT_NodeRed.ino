/*
 * โปรแกรมอ่านค่าเซ็นเซอร์ 2 ตัว แสดงผลบน Serial Monitor
 * - MAX30102: วัดอัตราการเต้นของหัวใจและความอิ่มตัวออกซิเจนในเลือด
 * - INA219: วัดแรงดันและกระแส
 */
#include <WiFi.h>
#include <PubSubClient.h>
// ========== INCLUDE LIBRARIES (เรียกใช้ไลบรารี่) ==========
#include <Arduino.h>          // ไลบรารี่หลักของ Arduino
#include <Wire.h>             // ไลบรารี่สำหรับการสื่อสาร I2C
#include "MAX30105.h"         // ไลบรารี่สำหรับเซ็นเซอร์ MAX30102
#include "spo2_algorithm.h"   // ไลบรารี่คำนวณค่า SpO2 และ Heart Rate
#include <Adafruit_INA219.h>  // ไลบรารี่สำหรับเซ็นเซอร์ INA219

// หมายเหตุ: I2C สำหรับ MAX30102 และ INA219
// ESP32 ใช้ SDA=21, SCL=22 เป็นค่า default (ไม่ต้องกำหนด)
// ========== SENSOR OBJECTS (สร้างออบเจ็กต์เซ็นเซอร์) ==========
MAX30105 particleSensor;  // สร้างออบเจ็กต์สำหรับ MAX30102
Adafruit_INA219 ina219;   // สร้างออบเจ็กต์สำหรับ INA219

// ========== MAX30102 VARIABLES (ตัวแปรสำหรับ MAX30102) ==========
#define MAX_BRIGHTNESS 255  // ความสว่างสูงสุดของ LED
uint32_t irBuffer[100];     // บัฟเฟอร์เก็บข้อมูล IR 100 ตัวอย่าง
uint32_t redBuffer[100];    // บัฟเฟอร์เก็บข้อมูล Red LED 100 ตัวอย่าง
int32_t bufferLength;       // ความยาวของบัฟเฟอร์
int32_t spo2;               // ค่า SpO2 (ความอิ่มตัวออกซิเจน)
int8_t validSPO2;           // สถานะความถูกต้องของค่า SpO2 (1=ถูกต้อง, 0=ไม่ถูกต้อง)
int32_t heartRate;          // ค่าอัตราการเต้นหัวใจ (BPM)
int8_t validHeartRate;      // สถานะความถูกต้องของค่า Heart Rate

// ========== TIMING VARIABLES (ตัวแปรสำหรับจับเวลา) ==========
unsigned long lastMAX30102Read = 0;  // เวลาล่าสุดที่อ่าน MAX30102
unsigned long lastINA219Read = 0;    // เวลาล่าสุดที่อ่าน INA219

// กำหนดช่วงเวลาในการอ่านแต่ละเซ็นเซอร์ (มิลลิวินาที)
const unsigned long MAX30102_INTERVAL = 30000;  // อ่านทุก 30 วินาที
const unsigned long INA219_INTERVAL = 2000;     // อ่านทุก 2 วินาที

const char* ssid = "HACKATHON@WIFI";
const char* password = "123456789";

const char* mqtt_server = "45.136.253.72";
const int mqtt_port = 1883;

const char* mqtt_client_id = "ESP32_Client_winkawin";
const char* mqtt_topic_sub = "esp32/control/winkawin";
const char* mqtt_topic_pub_hr = "hr/winkawin";
const char* mqtt_topic_pub_power = "power/winkawin";

WiFiClient espClient;
PubSubClient client(espClient);

// ========== SETUP FUNCTION (ฟังก์ชันเริ่มต้น - รันครั้งเดียว) ==========
void setup() {
  Serial.begin(115200);  // เริ่มต้น Serial communication ที่ความเร็ว 115200 baud
  delay(2000);           // รอ 2 วินาทีให้ระบบเสถียร

  // แสดงข้อความเริ่มต้น
  Serial.println("\n\n========================================");
  Serial.println("  Combined Sensor System Starting...");
  Serial.println("========================================\n");

  // ========== INITIALIZE I2C (เริ่มต้นการสื่อสาร I2C) ==========
  Wire.begin();  // เริ่มต้น I2C (ใช้ pin default: SDA=21, SCL=22)

  // ========== INITIALIZE MAX30102 (เริ่มต้นเซ็นเซอร์ MAX30102) ==========
  Serial.println("[1/2] Initializing MAX30102 sensor...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {        // เริ่มต้นเซ็นเซอร์ด้วย I2C แบบเร็ว
    Serial.println("❌ MAX30102 not found! Check wiring.");  // ถ้าหาเซ็นเซอร์ไม่เจอ
  } else {
    Serial.println("✓ MAX30102 initialized successfully");
    particleSensor.setup();                     // ตั้งค่าเซ็นเซอร์เป็นค่า default
    particleSensor.setPulseAmplitudeRed(0x0A);  // ตั้งความแรงของ LED แดง
    particleSensor.setPulseAmplitudeGreen(0);   // ปิด LED เขียว (ไม่ใช้)
    Serial.println("   Place finger on sensor with steady pressure\n");
  }

  // ========== INITIALIZE INA219 (เริ่มต้นเซ็นเซอร์ INA219) ==========
  Serial.println("[2/2] Initializing INA219 sensor...");
  if (!ina219.begin()) {                                  // พยายามเชื่อมต่อกับ INA219
    Serial.println("❌ INA219 not found! Check wiring.");  // ถ้าหาเซ็นเซอร์ไม่เจอ
  } else {
    Serial.println("✓ INA219 initialized successfully");
    // สามารถตั้งค่าช่วงวัดได้ เช่น: ina219.setCalibration_32V_2A();
    Serial.println();
  }

  // แสดงข้อความว่าพร้อมใช้งาน
  Serial.println("========================================");
  Serial.println("  All Sensors Ready!");
  Serial.println("========================================\n");

  delay(1000);  // รอ 1 วินาที

  Connect_WiFi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ========== FUNCTION: READ MAX30102 (ฟังก์ชันอ่านค่า MAX30102) ==========
void readMAX30102() {
  Serial.println("\n>>> Reading MAX30102...");

  bufferLength = 100;  // กำหนดจำนวนตัวอย่างเป็น 100

  // อ่านข้อมูล 100 ตัวอย่างแรก
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)  // รอจนกว่าจะมีข้อมูลใหม่
      particleSensor.check();                    // ตรวจสอบข้อมูล

    redBuffer[i] = particleSensor.getRed();  // อ่านค่า LED แดง
    irBuffer[i] = particleSensor.getIR();    // อ่านค่า LED IR
    particleSensor.nextSample();             // ไปยังตัวอย่างถัดไป
  }

  // คำนวณค่า Heart Rate และ SpO2 จาก algorithm
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer,
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);

  // เลื่อนข้อมูลเก่าออก และเก็บข้อมูลใหม่ 25 ตัวอย่าง
  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];  // เลื่อนข้อมูล Red
    irBuffer[i - 25] = irBuffer[i];    // เลื่อนข้อมูล IR
  }

  // อ่านข้อมูลใหม่ 25 ตัวอย่างเพิ่มเติม
  for (byte i = 75; i < 100; i++) {
    while (particleSensor.available() == false)  // รอจนกว่าจะมีข้อมูลใหม่
      particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // คำนวณค่าใหม่อีกครั้งเพื่อความแม่นยำ
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer,
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);

  // แสดงผลลัพธ์
  Serial.println("--- MAX30102 Results ---");
  if (validHeartRate == 1 && validSPO2 == 1) {  // ถ้าค่าที่อ่านได้ถูกต้อง
    Serial.print("Heart Rate: ");
    Serial.print(heartRate);
    Serial.println(" BPM");

    Serial.print("SpO2:       ");
    Serial.print(spo2);
    Serial.println(" %");

    String msg_hr = "{";
    msg_hr = msg_hr + " \"hr\": ";
    msg_hr = msg_hr + String(heartRate) + " , ";

    msg_hr = msg_hr + " \"spo2\": ";
    msg_hr = msg_hr + String(spo2);

    msg_hr = msg_hr + "}";

    client.publish(mqtt_topic_pub_hr, msg_hr.c_str());
    Serial.print("Published message: " + msg_hr);
    Serial.print("  | to -> TOPIC: ");
    Serial.println(mqtt_topic_pub_hr);
    msg_hr = "";


  } else {  // ถ้าค่าไม่ถูกต้อง
    Serial.println("⚠ Invalid readings - Check finger placement");
    Serial.println("Heart Rate: -- BPM");
    Serial.println("SpO2:       -- %");
  }
  Serial.println();
}

// ========== FUNCTION: READ INA219 (ฟังก์ชันอ่านค่า INA219) ==========
void readINA219() {
  Serial.println("--- INA219 Power Monitor ---");

  // อ่านค่าต่างๆ จากเซ็นเซอร์
  float shuntvoltage = ina219.getShuntVoltage_mV();        // แรงดันตกคร่อม Shunt Resistor (mV)
  float busvoltage = ina219.getBusVoltage_V();             // แรงดันไฟที่ Bus (V)
  float current_mA = ina219.getCurrent_mA();               // กระแสไฟ (mA)
  float power_mW = ina219.getPower_mW();                   // กำลังไฟฟ้า (mW)
  float loadvoltage = busvoltage + (shuntvoltage / 1000);  // แรงดันที่ Load (V)

  // แสดงผลทุกค่าที่วัดได้
  Serial.print("Bus Voltage:   ");
  Serial.print(busvoltage, 3);  // แสดงทศนิยม 3 ตำแหน่ง
  Serial.println(" V");

  Serial.print("Shunt Voltage: ");
  Serial.print(shuntvoltage, 3);
  Serial.println(" mV");

  Serial.print("Load Voltage:  ");
  Serial.print(loadvoltage, 3);
  Serial.println(" V");

  Serial.print("Current:       ");
  Serial.print(current_mA, 2);  // แสดงทศนิยม 2 ตำแหน่ง
  Serial.println(" mA");

  Serial.print("Power:         ");
  Serial.print(power_mW, 2);
  Serial.println(" mW");

  Serial.println();

  String msg_power = "{";
  msg_power = msg_power + " \"busvoltage\": ";
  msg_power = msg_power + String(busvoltage) + " , ";

  msg_power = msg_power + " \"shuntvoltage\": ";
  msg_power = msg_power + String(shuntvoltage) + " , ";

  msg_power = msg_power + " \"loadvoltage\": ";
  msg_power = msg_power + String(loadvoltage) + " , ";

  msg_power = msg_power + " \"current_mA\": ";
  msg_power = msg_power + String(current_mA) + " , ";

  msg_power = msg_power + " \"power_mW\": ";
  msg_power = msg_power + String(power_mW);

  msg_power = msg_power + "}";

  client.publish(mqtt_topic_pub_power, msg_power.c_str());
  Serial.print("Published message: " + msg_power);
  Serial.print("  | to -> TOPIC: ");
  Serial.println(mqtt_topic_pub_power);
  msg_power = "";
}

void Connect_WiFi() {
  Serial.print(" Connecting to WiFi... ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" Connected to WiFi ");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String txt_message = "";
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] :   ");
  for (int i = 0; i < length; i++) {
    txt_message += (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();
  txt_message.trim();
  txt_message = "";
}

void Connect_MQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("Connected to MQTT Broker");
      client.subscribe(mqtt_topic_sub);
    } else {
      Serial.print("Failed ,  rc=");
      Serial.print(client.state());
      Serial.println(" Try again in 3 seconds.");
      delay(3000);
    }
  }
}

// ========== LOOP FUNCTION (ฟังก์ชันหลัก - รันซ้ำตลอด) ==========
void loop() {

  if (!client.connected()) {
    Connect_MQTT();
  }
  client.loop();

  if (millis() - lastMAX30102Read > MAX30102_INTERVAL) {
    lastMAX30102Read = millis();
    readMAX30102();
  }

  if (millis() - lastINA219Read > INA219_INTERVAL) {
    lastINA219Read = millis();
    readINA219();
  }
}