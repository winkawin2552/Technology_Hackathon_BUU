/*
 * โปรแกรมอ่านค่าเซ็นเซอร์ 3 ตัว แสดงผลบน Serial Monitor
 * - MAX30102: วัดอัตราการเต้นของหัวใจและความอิ่มตัวออกซิเจนในเลือด
 * - INA219: วัดแรงดันและกระแส
 * - ADXL335: วัดความเร่ง 3 แกน
 */

// ========== INCLUDE LIBRARIES (เรียกใช้ไลบรารี่) ==========
#include <Arduino.h>              // ไลบรารี่หลักของ Arduino
#include <Wire.h>                 // ไลบรารี่สำหรับการสื่อสาร I2C
#include "MAX30105.h"             // ไลบรารี่สำหรับเซ็นเซอร์ MAX30102
#include "spo2_algorithm.h"       // ไลบรารี่คำนวณค่า SpO2 และ Heart Rate
#include <Adafruit_INA219.h>      // ไลบรารี่สำหรับเซ็นเซอร์ INA219

// ========== PIN DEFINITIONS (กำหนดหมายเลขขา) ==========
// ขาสำหรับเซ็นเซอร์ ADXL335 (Accelerometer)
#define X_PIN 25                  // ขาสำหรับอ่านค่าแกน X
#define Y_PIN 26                  // ขาสำหรับอ่านค่าแกน Y
#define Z_PIN 27                  // ขาสำหรับอ่านค่าแกน Z

// หมายเหตุ: I2C สำหรับ MAX30102 และ INA219
// ESP32 ใช้ SDA=21, SCL=22 เป็นค่า default (ไม่ต้องกำหนด)

// ========== SENSOR OBJECTS (สร้างออบเจ็กต์เซ็นเซอร์) ==========
MAX30105 particleSensor;          // สร้างออบเจ็กต์สำหรับ MAX30102
Adafruit_INA219 ina219;           // สร้างออบเจ็กต์สำหรับ INA219

// ========== MAX30102 VARIABLES (ตัวแปรสำหรับ MAX30102) ==========
#define MAX_BRIGHTNESS 255        // ความสว่างสูงสุดของ LED
uint32_t irBuffer[100];           // บัฟเฟอร์เก็บข้อมูล IR 100 ตัวอย่าง
uint32_t redBuffer[100];          // บัฟเฟอร์เก็บข้อมูล Red LED 100 ตัวอย่าง
int32_t bufferLength;             // ความยาวของบัฟเฟอร์
int32_t spo2;                     // ค่า SpO2 (ความอิ่มตัวออกซิเจน)
int8_t validSPO2;                 // สถานะความถูกต้องของค่า SpO2 (1=ถูกต้อง, 0=ไม่ถูกต้อง)
int32_t heartRate;                // ค่าอัตราการเต้นหัวใจ (BPM)
int8_t validHeartRate;            // สถานะความถูกต้องของค่า Heart Rate

// ========== ADXL335 VARIABLES (ตัวแปรสำหรับ ADXL335) ==========
float x_zero = 1.499;             // แรงดันเมื่อแกน X = 0g (จากการคาลิเบรต)
float y_zero = 1.505;             // แรงดันเมื่อแกน Y = 0g (จากการคาลิเบรต)
float z_zero = 1.528;             // แรงดันเมื่อแกน Z = 0g (จากการคาลิเบรต)
float sensitivity = 0.300;        // ความไวของเซ็นเซอร์ (V/g)
bool calibrated = true;           // สถานะว่าได้คาลิเบรตแล้วหรือไม่

// ตัวแปรเก็บค่าความเร่งปัจจุบัน
float current_x_g = 0;            // ความเร่งแกน X (g)
float current_y_g = 0;            // ความเร่งแกน Y (g)
float current_z_g = 0;            // ความเร่งแกน Z (g)
float current_total_g = 0;        // ความเร่งรวมทั้ง 3 แกน (g)

// ========== TIMING VARIABLES (ตัวแปรสำหรับจับเวลา) ==========
unsigned long lastMAX30102Read = 0;   // เวลาล่าสุดที่อ่าน MAX30102
unsigned long lastINA219Read = 0;     // เวลาล่าสุดที่อ่าน INA219
unsigned long lastADXL335Read = 0;    // เวลาล่าสุดที่อ่าน ADXL335

// กำหนดช่วงเวลาในการอ่านแต่ละเซ็นเซอร์ (มิลลิวินาที)
const unsigned long MAX30102_INTERVAL = 30000;  // อ่านทุก 30 วินาที
const unsigned long INA219_INTERVAL = 5000;     // อ่านทุก 5 วินาที
const unsigned long ADXL335_INTERVAL = 200;     // อ่านทุก 0.2 วินาที (200ms)

// ========== SETUP FUNCTION (ฟังก์ชันเริ่มต้น - รันครั้งเดียว) ==========
void setup() {
  Serial.begin(115200);           // เริ่มต้น Serial communication ที่ความเร็ว 115200 baud
  delay(2000);                    // รอ 2 วินาทีให้ระบบเสถียร
  
  // แสดงข้อความเริ่มต้น
  Serial.println("\n\n========================================");
  Serial.println("  Combined Sensor System Starting...");
  Serial.println("========================================\n");
  
  // ========== INITIALIZE I2C (เริ่มต้นการสื่อสาร I2C) ==========
  Wire.begin();                   // เริ่มต้น I2C (ใช้ pin default: SDA=21, SCL=22)
  
  // ========== INITIALIZE MAX30102 (เริ่มต้นเซ็นเซอร์ MAX30102) ==========
  Serial.println("[1/3] Initializing MAX30102 sensor...");
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {  // เริ่มต้นเซ็นเซอร์ด้วย I2C แบบเร็ว
    Serial.println("❌ MAX30102 not found! Check wiring."); // ถ้าหาเซ็นเซอร์ไม่เจอ
  } else {
    Serial.println("✓ MAX30102 initialized successfully");
    particleSensor.setup();                         // ตั้งค่าเซ็นเซอร์เป็นค่า default
    particleSensor.setPulseAmplitudeRed(0x0A);     // ตั้งความแรงของ LED แดง
    particleSensor.setPulseAmplitudeGreen(0);      // ปิด LED เขียว (ไม่ใช้)
    Serial.println("   Place finger on sensor with steady pressure\n");
  }
  
  // ========== INITIALIZE INA219 (เริ่มต้นเซ็นเซอร์ INA219) ==========
  Serial.println("[2/3] Initializing INA219 sensor...");
  if (!ina219.begin()) {          // พยายามเชื่อมต่อกับ INA219
    Serial.println("❌ INA219 not found! Check wiring."); // ถ้าหาเซ็นเซอร์ไม่เจอ
  } else {
    Serial.println("✓ INA219 initialized successfully");
    // สามารถตั้งค่าช่วงวัดได้ เช่น: ina219.setCalibration_32V_2A();
    Serial.println();
  }
  
  // ========== INITIALIZE ADXL335 (เริ่มต้นเซ็นเซอร์ ADXL335) ==========
  Serial.println("[3/3] Initializing ADXL335 accelerometer...");
  analogReadResolution(12);       // ตั้งค่า ADC ให้อ่านได้ละเอียด 12-bit (0-4095)
  analogSetAttenuation(ADC_11db); // ตั้งค่าช่วงวัดแรงดันเป็น 0-3.3V
  Serial.println("✓ ADXL335 initialized successfully");
  
  // แสดงค่าคาลิเบรตที่ตั้งไว้
  Serial.println("--- Using Pre-Calibrated Values ---");
  Serial.print("   X Zero-G: "); Serial.print(x_zero, 3); Serial.println(" V");
  Serial.print("   Y Zero-G: "); Serial.print(y_zero, 3); Serial.println(" V");
  Serial.print("   Z Zero-G: "); Serial.print(z_zero, 3); Serial.println(" V");
  Serial.print("   Sensitivity: "); Serial.print(sensitivity, 3); Serial.println(" V/g");
  Serial.println("   (Send 'c' to recalibrate)\n");
  
  // แสดงข้อความว่าพร้อมใช้งาน
  Serial.println("========================================");
  Serial.println("  All Sensors Ready!");
  Serial.println("========================================\n");
  
  delay(1000);                    // รอ 1 วินาที
}

// ========== FUNCTION: READ MAX30102 (ฟังก์ชันอ่านค่า MAX30102) ==========
void readMAX30102() {
  Serial.println("\n>>> Reading MAX30102...");
  
  bufferLength = 100;             // กำหนดจำนวนตัวอย่างเป็น 100
  
  // อ่านข้อมูล 100 ตัวอย่างแรก
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)  // รอจนกว่าจะมีข้อมูลใหม่
      particleSensor.check();                    // ตรวจสอบข้อมูล
    
    redBuffer[i] = particleSensor.getRed();      // อ่านค่า LED แดง
    irBuffer[i] = particleSensor.getIR();        // อ่านค่า LED IR
    particleSensor.nextSample();                 // ไปยังตัวอย่างถัดไป
  }
  
  // คำนวณค่า Heart Rate และ SpO2 จาก algorithm
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, 
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);
  
  // เลื่อนข้อมูลเก่าออก และเก็บข้อมูลใหม่ 25 ตัวอย่าง
  for (byte i = 25; i < 100; i++) {
    redBuffer[i - 25] = redBuffer[i];            // เลื่อนข้อมูล Red
    irBuffer[i - 25] = irBuffer[i];              // เลื่อนข้อมูล IR
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
  } else {                                        // ถ้าค่าไม่ถูกต้อง
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
  float shuntvoltage = ina219.getShuntVoltage_mV();  // แรงดันตกคร่อม Shunt Resistor (mV)
  float busvoltage = ina219.getBusVoltage_V();       // แรงดันไฟที่ Bus (V)
  float current_mA = ina219.getCurrent_mA();         // กระแสไฟ (mA)
  float power_mW = ina219.getPower_mW();             // กำลังไฟฟ้า (mW)
  float loadvoltage = busvoltage + (shuntvoltage / 1000);  // แรงดันที่ Load (V)
  
  // แสดงผลทุกค่าที่วัดได้
  Serial.print("Bus Voltage:   ");
  Serial.print(busvoltage, 3);                       // แสดงทศนิยม 3 ตำแหน่ง
  Serial.println(" V");
  
  Serial.print("Shunt Voltage: ");
  Serial.print(shuntvoltage, 3);
  Serial.println(" mV");
  
  Serial.print("Load Voltage:  ");
  Serial.print(loadvoltage, 3);
  Serial.println(" V");
  
  Serial.print("Current:       ");
  Serial.print(current_mA, 2);                       // แสดงทศนิยม 2 ตำแหน่ง
  Serial.println(" mA");
  
  Serial.print("Power:         ");
  Serial.print(power_mW, 2);
  Serial.println(" mW");
  
  Serial.println();
}

// ========== FUNCTION: READ ADXL335 (ฟังก์ชันอ่านค่า ADXL335) ==========
void readADXL335() {
  // อ่านค่า Analog จาก 3 แกน (ค่า 0-4095 จาก 12-bit ADC)
  int x = analogRead(X_PIN);      // อ่านค่าแกน X
  int y = analogRead(Y_PIN);      // อ่านค่าแกน Y
  int z = analogRead(Z_PIN);      // อ่านค่าแกน Z
  
  // แปลงค่า Analog เป็นแรงดัน (Voltage)
  float x_v = x * (3.3 / 4095.0); // แปลงเป็น Volt (0-3.3V)
  float y_v = y * (3.3 / 4095.0);
  float z_v = z * (3.3 / 4095.0);
  
  // คำนวณค่าความเร่ง (g) โดยใช้สูตร: g = (V - V_zero) / sensitivity
  current_x_g = (x_v - x_zero) / sensitivity;
  current_y_g = (y_v - y_zero) / sensitivity;
  current_z_g = (z_v - z_zero) / sensitivity;
  
  // คำนวณความเร่งรวม (Total G) = √(x² + y² + z²)
  current_total_g = sqrt(current_x_g*current_x_g + 
                        current_y_g*current_y_g + 
                        current_z_g*current_z_g);
  
  // แสดงผลทั้ง 3 รูปแบบ: Raw (ค่าดิบ), V (แรงดัน), G (ความเร่ง)
  Serial.print("ADXL335 | Raw: ");
  Serial.print(x); Serial.print(" ");
  Serial.print(y); Serial.print(" ");
  Serial.print(z);
  
  Serial.print(" | V: ");
  Serial.print(x_v, 2); Serial.print(" ");     // แสดงทศนิยม 2 ตำแหน่ง
  Serial.print(y_v, 2); Serial.print(" ");
  Serial.print(z_v, 2);
  
  Serial.print(" | G: ");
  Serial.print(current_x_g, 2); Serial.print(" ");
  Serial.print(current_y_g, 2); Serial.print(" ");
  Serial.print(current_z_g, 2);
  
  Serial.print(" | Total: ");
  Serial.println(current_total_g, 2);
}

// ========== FUNCTION: CALIBRATE (ฟังก์ชันคาลิเบรตเซ็นเซอร์) ==========
void calibrate() {
  Serial.println("\n*** Starting ADXL335 Calibration ***");
  Serial.println("Please place sensor FLAT with Z-axis pointing UP");
  Serial.println("Keep it still...");
  delay(2000);                    // รอให้ผู้ใช้วางเซ็นเซอร์

  // ตัวแปรสำหรับเก็บผลรวมของค่าที่อ่านได้
  float x_sum = 0, y_sum = 0, z_sum = 0;
  int samples = 100;              // จำนวนตัวอย่างที่จะอ่าน
  
  Serial.print("Reading values ");
  for(int i = 0; i < samples; i++) {
    // อ่านค่าและแปลงเป็น Voltage ทันที
    int x = analogRead(X_PIN);
    int y = analogRead(Y_PIN);
    int z = analogRead(Z_PIN);
    
    x_sum += x * (3.3 / 4095.0);  // รวมค่า X
    y_sum += y * (3.3 / 4095.0);  // รวมค่า Y
    z_sum += z * (3.3 / 4095.0);  // รวมค่า Z
    
    if(i % 10 == 0) Serial.print(".");  // แสดงจุดทุกๆ 10 ตัวอย่าง
    delay(20);                    // รอระหว่างการอ่านแต่ละครั้ง
  }
  Serial.println("");
  
  // คำนวณค่าเฉลี่ย
  x_zero = x_sum / samples;       // แรงดันเฉลี่ยของแกน X เมื่อ 0g
  y_zero = y_sum / samples;       // แรงดันเฉลี่ยของแกน Y เมื่อ 0g
  float z_one_g = z_sum / samples;  // แรงดันของแกน Z เมื่อ +1g (ชี้ขึ้น)
  
  // คำนวณความไว (Sensitivity)
  sensitivity = z_one_g - z_zero; // ความต่างของแรงดันเมื่อเปลี่ยน 1g
  z_zero = z_one_g - sensitivity; // ปรับค่า z_zero
  
  // แสดงผลการคาลิเบรต
  Serial.println("--- Calibration Complete ---");
  Serial.print("X Zero-G: "); Serial.print(x_zero, 3); Serial.println(" V");
  Serial.print("Y Zero-G: "); Serial.print(y_zero, 3); Serial.println(" V");
  Serial.print("Z Zero-G: "); Serial.print(z_zero, 3); Serial.println(" V");
  Serial.print("Z at +1g: "); Serial.print(z_one_g, 3); Serial.println(" V");
  Serial.print("Sensitivity: "); Serial.print(sensitivity, 3); Serial.println(" V/g");
  Serial.println("----------------------------");
  Serial.println("");
  
  calibrated = true;              // ตั้งสถานะว่าคาลิเบรตแล้ว
}

// ========== LOOP FUNCTION (ฟังก์ชันหลัก - รันซ้ำตลอด) ==========
void loop() {
  unsigned long currentTime = millis();  // เวลาปัจจุบัน (มิลลิวินาที)
  
  // ========== READ MAX30102 (ทุก 30 วินาที) ==========
  if (currentTime - lastMAX30102Read >= MAX30102_INTERVAL) {
    lastMAX30102Read = currentTime;      // บันทึกเวลาที่อ่านล่าสุด
    readMAX30102();                      // เรียกฟังก์ชันอ่านค่า MAX30102
  }
  
  // ========== READ INA219 (ทุก 5 วินาที) ==========
  if (currentTime - lastINA219Read >= INA219_INTERVAL) {
    lastINA219Read = currentTime;        // บันทึกเวลาที่อ่านล่าสุด
    readINA219();                        // เรียกฟังก์ชันอ่านค่า INA219
  }
  
  // ========== READ ADXL335 (ทุก 200 มิลลิวินาที) ==========
  if (currentTime - lastADXL335Read >= ADXL335_INTERVAL) {
    lastADXL335Read = currentTime;       // บันทึกเวลาที่อ่านล่าสุด
    readADXL335();                       // เรียกฟังก์ชันอ่านค่า ADXL335
  }
  
  // ========== ตรวจสอบคำสั่งจากผู้ใช้ ==========
  if(Serial.available()) {               // ถ้ามีข้อมูลเข้ามาทาง Serial
    char cmd = Serial.read();            // อ่านตัวอักษร
    if(cmd == 'c' || cmd == 'C') {      // ถ้าเป็น 'c' หรือ 'C'
      calibrate();                       // เรียกฟังก์ชันคาลิเบรต
    }
  }
  
  delay(1);                              // หน่วงเวลาเล็กน้อยเพื่อความเสถียร
}