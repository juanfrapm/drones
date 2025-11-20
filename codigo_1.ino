#include <WiFi.h>

#include <Wire.h>
// --- Datos IMU ---
float RateRoll, RatePitch, RateYaw;
float AccX, AccY, AccZ;
float AngleRoll, AnglePitch;
float LoopTimer;

// --- Datos WiFi ---
const char* ssid = "juanfra";
const char* password = "juanfrapm";
// ------------------

const int ledPin = 4;

WiFiServer server(80);

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
  Serial.begin(115200);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
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

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();

  // Semilla para el random
  randomSeed(analogRead(0));
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi caído. Reiniciando...");
    delay(1000);
    ESP.restart();
  }

  WiFiClient client = server.available();

  if (client) {
    String req = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        req += c;

        if (c == '\n') {
          // --- RUTA PRINCIPAL "/" ---
          if (req.indexOf("GET / ") >= 0) {

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.println("<html><head>");
            client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");

            // JavaScript para actualizar datos
            client.println("<script>");
            client.println("function updateData() {");
            client.println("  fetch('/data').then(r => r.text()).then(t => {");
            client.println("    document.getElementById('dato').innerHTML = t;");
            client.println("  });");
            client.println("}");
            client.println("setInterval(updateData, 1000);");
            client.println("window.onload = updateData;");
            client.println("</script>");

            client.println("</head><body style='text-align:center;'>");
            client.println("<h1>ESP32 - Control y Datos</h1>");

            // Botones LED
            client.println("<button onclick=\"fetch('/led/on')\" style='padding:15px; background:green; color:white;'>LED ON</button>");
            client.println("<button onclick=\"fetch('/led/off')\" style='padding:15px; background:red; color:white;'>LED OFF</button>");

            // Dato enviado por ESP32
            client.println("<h2>Dato recibido desde la ESP32:</h2>");
            client.println("<h1 id='dato'>--</h1>");

            client.println("</body></html>");
            break;
          }

          // --- RUTA PARA EL LED ---
          if (req.indexOf("GET /led/on") >= 0) {
            digitalWrite(ledPin, HIGH);

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();
            client.println("LED encendido");
            break;
          }

          if (req.indexOf("GET /led/off") >= 0) {
            digitalWrite(ledPin, LOW);

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println("Connection: close");
            client.println();
            client.println("LED apagado");
            break;
          }

          // --- RUTA PARA DATO ALEATORIO ---
          if (req.indexOf("GET /data") >= 0) {
            int numero = random(0, 100);  // número aleatorio entre 0 y 99
            gyro_signals();
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println();
            client.println(AngleRoll);
            break;
          }

          req = "";
        }
      }
    }
    client.stop();
  }
}
