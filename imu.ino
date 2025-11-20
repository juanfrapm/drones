//taken from Carbon Aeronautics
#include <Wire.h>
float RateRoll, RatePitch, RateYaw;
float AccX, AccY, AccZ;
float AngleRoll, AnglePitch;
float LoopTimer;
int led = 4;           // the PWM pin the LED is attached to

void gyro_signals(void) {
  Wire.beginTransmission(0x68);
  Wire.write(0x1A);
  Wire.write(0x05);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x1C);
  Wire.write(0x10);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x3B);
  Wire.endTransmission(); 
  Wire.requestFrom(0x68, 6);

  int16_t AccXLSB = Wire.read() << 8 | Wire.read();
  int16_t AccYLSB = Wire.read() << 8 | Wire.read();
  int16_t AccZLSB = Wire.read() << 8 | Wire.read();

  Wire.beginTransmission(0x68);
  Wire.write(0x1B); 
  Wire.write(0x08);
  Wire.endTransmission();                                                   

  Wire.beginTransmission(0x68);
  Wire.write(0x43);
  Wire.endTransmission();
  Wire.requestFrom(0x68, 6);

  int16_t GyroX = Wire.read() << 8 | Wire.read();
  int16_t GyroY = Wire.read() << 8 | Wire.read();
  int16_t GyroZ = Wire.read() << 8 | Wire.read();

  RateRoll  = (float)GyroX / 65.5;
  RatePitch = (float)GyroY / 65.5;
  RateYaw   = (float)GyroZ / 65.5;

  AccX = (float)AccXLSB / 4096;
  AccY = (float)AccYLSB / 4096;
  AccZ = (float)AccZLSB / 4096;

  AngleRoll  = atan(AccY / sqrt(AccX*AccX + AccZ*AccZ)) * 180 / PI;
  AnglePitch = -atan(AccX / sqrt(AccY*AccY + AccZ*AccZ)) * 180 / PI;
}

void setup() {
  Serial.begin(57600);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(led, OUTPUT);
  // 🔥 CONFIGURAR I2C PARA LA ESP32-CAM
  // SDA = GPIO 12
  // SCL = GPIO 13
  Wire.begin(14, 15);

  Wire.setClock(400000);
  delay(250);

  Wire.beginTransmission(0x68); 
  Wire.write(0x6B);
  Wire.write(0x00);   // wake up the MPU6050
  Wire.endTransmission();
}

void loop() {
  gyro_signals();

  Serial.print("Acceleration X [g]= ");
  Serial.print(AccX);
  Serial.print(" Acceleration Y [g]= ");
  Serial.print(AccY);
  Serial.print(" Acceleration Z [g]= ");
  Serial.println(AccZ);

  Serial.print("Roll Angle= ");
  Serial.print(AngleRoll);
  Serial.print("  Pitch Angle= ");
  Serial.println(AnglePitch);
  analogWrite(led, AnglePitch);
  delay(50);
}
