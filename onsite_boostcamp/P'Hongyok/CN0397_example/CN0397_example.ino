// ========== INCLUDE LIBRARIES ==========
#include <SPI.h>               // ไลบรารีสำหรับการสื่อสาร SPI (Serial Peripheral Interface)
                               // SPI = โปรโตคอลสื่อสารแบบ synchronous ความเร็วสูง
                               // ใช้ 4 สาย: MOSI, MISO, SCK, CS
                               
#include "CN0397.h"            // ไลบรารีสำหรับควบคุม CN0397 Gas Sensor Module
                               // CN0397 = วงจรวัดก๊าซที่ใช้ Electrochemical Sensor
                               
#include <Arduino.h>           // ไลบรารีหลักของ Arduino Framework

// ========== PIN DEFINITIONS ==========
byte CS_PIN = 10;              // กำหนด GPIO 10 เป็นขา Chip Select (CS)
                               // byte = ตัวเลข 0-255 (8-bit unsigned)
                               // CS = สัญญาณเลือกอุปกรณ์ SPI (LOW = เลือก, HIGH = ไม่เลือก)
                               // ใน SPI สามารถมีหลายอุปกรณ์ต่อบัสเดียวกัน

// ========== SETUP FUNCTION ==========
void setup() {                 // ฟังก์ชันเริ่มต้น ทำงานครั้งเดียวตอนเปิดเครื่อง

  // ----- Serial Communication Setup -----
  Serial.begin(9600);          // เริ่มต้นการสื่อสาร Serial ที่ความเร็ว 9600 baud
                               // 9600 baud = 9600 bits/second
                               // ช้ากว่า 115200 แต่เสถียรกว่าสำหรับระยะทางไกล
  
  // ----- SPI Communication Setup -----
  SPI.begin();                 // เริ่มต้นการสื่อสาร SPI
                               // ตั้งค่าขาพื้นฐาน:
                               // - MOSI (Master Out Slave In): ส่งข้อมูล
                               // - MISO (Master In Slave Out): รับข้อมูล
                               // - SCK (Serial Clock): สัญญาณนาฬิกา
                               
  SPI.setDataMode(SPI_MODE3);  // ตั้งค่าโหมดการทำงานของ SPI เป็น MODE 3
                               // SPI_MODE3 หมายถึง:
                               // - CPOL (Clock Polarity) = 1: Clock idle เป็น HIGH
                               // - CPHA (Clock Phase) = 1: อ่านข้อมูลที่ขอบขาลง (falling edge)
                               // SPI มี 4 โหมด (MODE0-MODE3):
                               //   MODE0: CPOL=0, CPHA=0 (นิยมสุด)
                               //   MODE1: CPOL=0, CPHA=1
                               //   MODE2: CPOL=1, CPHA=0
                               //   MODE3: CPOL=1, CPHA=1 (ใช้กับ CN0397)
                               
  delay(1000);                 // หน่วงเวลา 1000 ms (1 วินาที)
                               // ให้เวลา SPI และ Serial เริ่มต้นเสร็จสมบูรณ์

  // ----- Chip Select Pin Setup -----
  pinMode(CS_PIN, OUTPUT);     // ตั้งค่า CS_PIN (GPIO 10) เป็น OUTPUT mode
                               // OUTPUT = ส่งสัญญาณออกไป (HIGH/LOW)
                               // CS ต้องเป็น OUTPUT เพื่อควบคุมการเลือกอุปกรณ์

  // ----- CN0397 Initialization -----
  CN0397_Init();               // เรียกฟังก์ชันเริ่มต้น CN0397 Gas Sensor
                               // ฟังก์ชันนี้อยู่ในไฟล์ CN0397.cpp/.h
                               // ทำหน้าที่:
                               // 1. ตั้งค่า ADC (Analog-to-Digital Converter)
                               // 2. กำหนดค่า Gain และ Range
                               // 3. ตั้งค่า Sampling Rate
                               // 4. เตรียมเซ็นเซอร์ให้พร้อมวัดก๊าซ
}

// ========== MAIN LOOP ==========
void loop() {                  // ฟังก์ชันหลัก วนซ้ำไม่รู้จบตลอดเวลา
  
  // ----- Refresh Delay -----
  delay(DISPLAY_REFRESH);      // หน่วงเวลาตามค่า DISPLAY_REFRESH
                               // DISPLAY_REFRESH = ค่าคงที่ที่กำหนดใน CN0397.h
                               // โดยปกติประมาณ 500-1000 ms
                               // ควบคุมความถี่ในการอัปเดตข้อมูล

  // ----- Update Sensor Data -----
  CN0397_SetAppData();         // เรียกฟังก์ชันอ่านและประมวลผลข้อมูลจากเซ็นเซอร์
                               // ฟังก์ชันนี้ทำหน้าที่:
                               // 1. อ่านค่าแรงดันจาก ADC ผ่าน SPI
                               // 2. แปลงค่าแรงดันเป็นค่าความเข้มข้นของก๊าซ (PPM)
                               // 3. คำนวณค่าอุณหภูมิ (ถ้ามี)
                               // 4. เก็บข้อมูลไว้ในตัวแปร global ภายใน library
                               // SetAppData = Set Application Data

  // ----- Display Data -----
  CN0397_DisplayData();        // เรียกฟังก์ชันแสดงผลข้อมูลบน Serial Monitor
                               // ฟังก์ชันนี้ทำหน้าที่:
                               // 1. ดึงข้อมูลที่ประมวลผลแล้วจาก SetAppData()
                               // 2. จัดรูปแบบข้อมูลให้อ่านง่าย
                               // 3. ส่งออกไปทาง Serial.print()
                               // แสดงข้อมูลเช่น:
                               // - Gas Concentration (PPM)
                               // - Sensor Voltage (V)
                               // - Temperature (°C)
                               // - Sensor Status
}