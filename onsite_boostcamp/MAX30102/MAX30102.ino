// ========== INCLUDE LIBRARIES ==========
#include <Wire.h>              // ไลบรารีสำหรับการสื่อสารแบบ I2C (Inter-Integrated Circuit)
#include "MAX30105.h"          // ไลบรารีควบคุมเซ็นเซอร์ MAX30105/MAX30102
#include "spo2_algorithm.h"    // ไลบรารีคำนวณค่า SpO2 และ Heart Rate จากข้อมูล LED

// ========== SENSOR OBJECT ==========
MAX30105 particleSensor;       // สร้างออบเจ็กต์สำหรับควบคุมเซ็นเซอร์ MAX30102

// ========== CONSTANT DEFINITIONS ==========
#define MAX_BRIGHTNESS 255     // ค่าความสว่างสูงสุดของ LED (0-255)

// ========== BUFFER ARRAYS ==========
uint32_t irBuffer[100];        // อาร์เรย์เก็บค่าจาก Infrared LED (100 ตัวอย่าง)
                               // uint32_t = ตัวเลขไม่มีเครื่องหมาย 32 บิต (0 ถึง 4,294,967,295)
                               
uint32_t redBuffer[100];       // อาร์เรย์เก็บค่าจาก Red LED (100 ตัวอย่าง)

// ========== CALCULATION VARIABLES ==========
int32_t bufferLength;          // ความยาวของข้อมูลที่จะใช้คำนวณ (ปกติคือ 100)
                               // int32_t = ตัวเลขมีเครื่องหมาย 32 บิต (-2.1B ถึง 2.1B)
                               
int32_t spo2;                  // ตัวแปรเก็บค่า SpO2 (ออกซิเจนในเลือด) หน่วยเปอร์เซ็นต์
                               
int8_t validSPO2;              // ตัวแปรบอกว่าค่า SpO2 ที่คำนวณได้ถูกต้องหรือไม่
                               // 1 = ถูกต้อง, 0 = ไม่ถูกต้อง
                               // int8_t = ตัวเลขมีเครื่องหมาย 8 บิต (-128 ถึง 127)
                               
int32_t heartRate;             // ตัวแปรเก็บค่า Heart Rate (อัตราการเต้นหัวใจ) หน่วย BPM
                               
int8_t validHeartRate;         // ตัวแปรบอกว่าค่า Heart Rate ที่คำนวณได้ถูกต้องหรือไม่
                               // 1 = ถูกต้อง, 0 = ไม่ถูกต้อง

// ========== SETUP FUNCTION ==========
void setup() {                 // ฟังก์ชันที่ทำงานครั้งเดียวตอนเริ่มต้นโปรแกรม
  
  // ----- Serial Communication Setup -----
  Serial.begin(115200);        // เริ่มต้นการสื่อสาร Serial ที่ความเร็ว 115200 baud
                               // baud = จำนวนบิตต่อวินาที ที่ส่งข้อมูล
                               
  Serial.println("Initializing MAX30102 sensor..."); 
                               // แสดงข้อความบน Serial Monitor บรรทัดใหม่

  // ----- Sensor Initialization -----
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) { 
                               // เริ่มต้นเซ็นเซอร์ผ่าน I2C ความเร็ว 400kHz
                               // Wire = ออบเจ็กต์ I2C เริ่มต้น
                               // I2C_SPEED_FAST = 400kHz (มาตรฐาน = 100kHz)
                               // ! = NOT (ถ้าเริ่มต้นไม่สำเร็จจะเข้า if)
                               
    Serial.println("MAX30105 was not found. Please check wiring/power.");
                               // แสดงข้อความเตือนว่าหาเซ็นเซอร์ไม่เจอ
                               
    while (1);                 // วนลูปไม่รู้จบ (หยุดโปรแกรม) เพราะเซ็นเซอร์ไม่พร้อม
  }

  Serial.println("Place your index finger on the sensor with steady pressure.");
                               // แจ้งให้ผู้ใช้วางนิ้วบนเซ็นเซอร์

  // ----- Sensor Configuration -----
  particleSensor.setup();      // ตั้งค่าเซ็นเซอร์ด้วยค่าเริ่มต้น
                               // - Sample rate: 25 samples/second
                               // - LED pulse width: 411μs
                               // - ADC range: 4096
                               
  particleSensor.setPulseAmplitudeRed(0x0A); 
                               // ตั้งกำลังของ Red LED เป็น 0x0A (10 ในเลขฐาน 10)
                               // ค่าต่ำเพื่อประหยัดพลังงานและบอกว่าเซ็นเซอร์ทำงาน
                               // 0x = เลขฐาน 16 (Hexadecimal)
                               
  particleSensor.setPulseAmplitudeGreen(0); 
                               // ปิด Green LED (ไม่ใช้ในการวัด SpO2)
}

// ========== ECG READING FUNCTION ==========
void readMAX30102() {          // ฟังก์ชันสำหรับอ่านและคำนวณค่าจากเซ็นเซอร์
  
  bufferLength = 100;          // กำหนดความยาว buffer = 100 ตัวอย่าง
                               // ที่ 25 samples/sec, 100 ตัวอย่าง = 4 วินาที

  // ----- Phase 1: Initial Sample Collection (4 seconds) -----
  for (byte i = 0; i < bufferLength; i++) { 
                               // วนลูป 100 รอบเพื่อเก็บข้อมูลเริ่มต้น
                               // byte = ตัวเลข 0-255 (ใช้หน่วยความจำน้อย)
                               
    while (particleSensor.available() == false) 
                               // รอจนกว่าจะมีข้อมูลใหม่พร้อม
                               // available() คืนค่า true เมื่อมีข้อมูลใหม่
                               
      particleSensor.check();  // เช็คว่ามีข้อมูลใหม่จากเซ็นเซอร์หรือยัง

    redBuffer[i] = particleSensor.getRed(); 
                               // อ่านค่าจาก Red LED และเก็บใน array ตำแหน่ง i
                               
    irBuffer[i] = particleSensor.getIR(); 
                               // อ่านค่าจาก IR LED และเก็บใน array ตำแหน่ง i
                               
    particleSensor.nextSample(); 
                               // บอกเซ็นเซอร์ว่าเราเสร็จแล้ว ให้เตรียมข้อมูลชุดถัดไป

    // ----- Print Raw Values -----
    Serial.print(F("red="));   // F() = เก็บข้อความใน Flash memory แทน RAM
                               // ช่วยประหยัด RAM
                               
    Serial.print(redBuffer[i], DEC); 
                               // แสดงค่า Red LED ในรูปแบบเลขฐาน 10
                               // DEC = Decimal (ฐาน 10)
                               
    Serial.print(F(", ir=")); 
    Serial.println(irBuffer[i], DEC); 
                               // println = print และขึ้นบรรทัดใหม่
  }

  // ----- Phase 2: Initial Calculation -----
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, 
                                         &spo2, &validSPO2, &heartRate, &validHeartRate);
                               // เรียกใช้ฟังก์ชันคำนวณ HR และ SpO2
                               // & = ส่งที่อยู่ของตัวแปร (pass by reference)
                               // ทำให้ฟังก์ชันสามารถเปลี่ยนค่าตัวแปรได้

  // ----- Phase 3: Continuous Sampling Loop -----
  while (1) {                  // ลูปไม่รู้จบ (infinite loop)
    
    // ----- Shift Old Data -----
    for (byte i = 25; i < 100; i++) { 
                               // วนลูปจากตำแหน่ง 25 ถึง 99
                               // เลื่อนข้อมูล 75 ตัวอย่างท้ายสุดมาด้านหน้า
                               
      redBuffer[i - 25] = redBuffer[i]; 
                               // ย้ายข้อมูลจากตำแหน่ง i ไปตำแหน่ง i-25
                               // เช่น i=25 → ไปที่ i=0, i=26 → ไปที่ i=1
                               
      irBuffer[i - 25] = irBuffer[i]; 
                               // ทำเช่นเดียวกันกับ IR buffer
    }
    // หลังลูปนี้: ตำแหน่ง 0-74 = ข้อมูลเก่า, 75-99 = ว่าง

    // ----- Collect 25 New Samples -----
    for (byte i = 75; i < 100; i++) { 
                               // วนลูป 25 รอบเพื่อเก็บข้อมูลใหม่
                               // เก็บในตำแหน่ง 75-99
                               
      while (particleSensor.available() == false) 
        particleSensor.check(); 
                               // รอข้อมูลใหม่เหมือนเดิม

      redBuffer[i] = particleSensor.getRed(); 
      irBuffer[i] = particleSensor.getIR(); 
      particleSensor.nextSample(); 
                               // อ่านค่าใหม่เหมือนเดิม

      // ----- Print Current Values -----
      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.print(irBuffer[i], DEC);
      Serial.print(F(", HR="));
      Serial.print(heartRate, DEC); 
                               // แสดงค่า Heart Rate ล่าสุดที่คำนวณได้
                               
      Serial.print(F(", HRvalid="));
      Serial.print(validHeartRate, DEC); 
                               // แสดงว่าค่า HR ถูกต้องหรือไม่ (1/0)
                               
      Serial.print(F(", SPO2="));
      Serial.print(spo2, DEC); 
                               // แสดงค่า SpO2 ล่าสุด
                               
      Serial.print(F(", SPO2Valid="));
      Serial.println(validSPO2, DEC); 
                               // แสดงว่าค่า SpO2 ถูกต้องหรือไม่
    }

    // ----- Recalculate with New Data -----
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, 
                                           &spo2, &validSPO2, &heartRate, &validHeartRate);
                               // คำนวณค่าใหม่โดยใช้ข้อมูล 100 ตัวอย่าง
                               // (75 เก่า + 25 ใหม่)
   
    break;                     // ออกจาก while(1) loop
                               // เพื่อกลับไปที่ main loop
  }
}

// ========== MAIN LOOP ==========
void loop() {                  // ฟังก์ชันหลักที่วนซ้ำไม่รู้จบ
  readMAX30102();              // เรียกใช้ฟังก์ชันอ่านเซ็นเซอร์
                               // จะอัปเดตค่าทุก 1 วินาที (25 ตัวอย่างใหม่)
}