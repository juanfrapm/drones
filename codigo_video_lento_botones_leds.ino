#include <WiFi.h>
#include "esp_camera.h"

// ---------- CONFIGURACIÓN WIFI ----------
const char* ssid     = "juanfra";
const char* password = "juanfrapm";

// ---------- PIN LED GPIO 4 ----------
const int ledPin = 4;

// ---------- SERVIDOR EN PUERTO 80 ----------
WiFiServer server(80);

// ---------- CONFIGURACIÓN DE LA CÁMARA (AI THINKER) ----------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ---------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // ---- Conectar a WiFi ----
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ---- Configurar cámara ----
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_QVGA;  
  config.jpeg_quality = 36; 
  config.fb_count     = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error iniciando la cámara");
    return;
  }

  server.begin();
}

// ---------------------------------------------------
// Función auxiliar para hacer parpadear el LED al recibir comando
void blinkLED() {
  digitalWrite(ledPin, HIGH);
  delay(100); // Breve encendido
  digitalWrite(ledPin, LOW);
}

// Función para interpretar los 6 botones
void ejecutarComando(String cmd) {
  // Detectar qué botón se presionó
  if (cmd.indexOf("GET /action/forward") >= 0) {
    Serial.println("ADELANTE");
    blinkLED();
  }
  else if (cmd.indexOf("GET /action/back") >= 0) {
    Serial.println("ATRAS");
    blinkLED();
  }
  else if (cmd.indexOf("GET /action/left") >= 0) {
    Serial.println("IZQUIERDA");
    blinkLED();
  }
  else if (cmd.indexOf("GET /action/right") >= 0) {
    Serial.println("DERECHA");
    blinkLED();
  }
  else if (cmd.indexOf("GET /action/up") >= 0) {
    Serial.println("ARRIBA");
    blinkLED();
  }
  else if (cmd.indexOf("GET /action/down") >= 0) {
    Serial.println("ABAJO");
    blinkLED();
  }
}

// ---------------------------------------------------
// STREAM MJPEG en /stream (CON LA SOLUCIÓN DE LOS BOTONES)
void enviarStream(WiFiClient &streamClient) {
  streamClient.println("HTTP/1.1 200 OK");
  streamClient.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  streamClient.println();

  const uint32_t FRAME_INTERVAL_MS = 200;  // 200 ms → ~5 FPS
  uint32_t lastFrameTime = 0;

  while (streamClient.connected()) {
    uint32_t now = millis();
    if (now - lastFrameTime < FRAME_INTERVAL_MS) {
      // Todavía no pasó el tiempo mínimo entre frames
      delay(1);   // cede CPU al WiFi/RTOS
      continue;
    }
    lastFrameTime = now;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Fallo captura de cámara");
      delay(10);
      continue;
    }

    streamClient.println("--frame");
    streamClient.println("Content-Type: image/jpeg");
    streamClient.printf("Content-Length: %d\r\n\r\n", fb->len);
    streamClient.write(fb->buf, fb->len);
    streamClient.println();
    
    esp_camera_fb_return(fb);
    yield();  // también ayuda a que no se congele

    // --- Revisar botones durante el video ---
    WiFiClient commandClient = server.available();
    if (commandClient) {
      String req = commandClient.readStringUntil('\r');
      commandClient.flush();
      ejecutarComando(req); // Procesar orden
      commandClient.println("HTTP/1.1 200 OK");
      commandClient.println("Connection: close");
      commandClient.println();
      commandClient.stop();
    }
  }
}

// ---------------------------------------------------
void loop() {
  WiFiClient client = server.available();
  
  if (client) {
    String req = client.readStringUntil('\r');
    client.flush();

    if (req.indexOf("GET /stream") >= 0) {
      enviarStream(client);
    }
    else {
      // Revisar si es un comando (por si no hay video)
      ejecutarComando(req);

      if (req.indexOf("GET / ") >= 0) {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();

        client.println("<html><head>");
        client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
        client.println("<style>");
        client.println("body { text-align:center; font-family: Arial; background-color: #222; color: white; }");
        client.println("button { padding: 15px; font-size: 18px; width: 100px; margin: 5px; border: none; border-radius: 8px; cursor: pointer; }");
        client.println(".dir { background-color: #007bff; color: white; }"); /* Azul para movimiento */
        client.println(".alt { background-color: #28a745; color: white; }"); /* Verde para altura */
        client.println("</style>");
        client.println("</head><body>");

        client.println("<h2>Control Drone ESP32</h2>");
        client.println("<img src='/stream' style='width:320px; border-radius:10px; border: 2px solid #555;'>");
        client.println("<br><br>");

        // --- CONTROLES DIRECCIÓN ---
        client.println("<div>");
        client.println("<button class='dir' onclick=\"fetch('/action/forward')\">Adelante</button><br>");
        client.println("<button class='dir' onclick=\"fetch('/action/left')\">Izquierda</button>");
        client.println("<button class='dir' onclick=\"fetch('/action/right')\">Derecha</button><br>");
        client.println("<button class='dir' onclick=\"fetch('/action/back')\">Atras</button>");
        client.println("</div>");
        
        client.println("<br><hr style='width:50%'><br>");

        // --- CONTROLES ALTURA ---
        client.println("<div>");
        client.println("<button class='alt' onclick=\"fetch('/action/up')\">Arriba</button>");
        client.println("<button class='alt' onclick=\"fetch('/action/down')\">Abajo</button>");
        client.println("</div>");

        client.println("</body></html>");
      }
      else {
        client.println("HTTP/1.1 200 OK");
        client.println("Connection: close");
        client.println();
      }
      client.stop();
    }
  }
}
