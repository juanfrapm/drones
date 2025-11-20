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

  // Bajamos un poco la calidad para ganar velocidad y respuesta
  config.frame_size   = FRAMESIZE_QVGA;  
  config.jpeg_quality = 12; 
  config.fb_count     = 2;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error iniciando la cámara");
    return;
  }

  server.begin();
}

// ---------------------------------------------------
// Función auxiliar para ejecutar comandos
void ejecutarComando(String cmd) {
  if (cmd.indexOf("GET /led/on") >= 0) {
    digitalWrite(ledPin, HIGH);
    Serial.println("LED ENCENDIDO");
  }
  else if (cmd.indexOf("GET /led/off") >= 0) {
    digitalWrite(ledPin, LOW);
    Serial.println("LED APAGADO");
  }
}

// ---------------------------------------------------
// STREAM MJPEG en /stream
// MODIFICADO: Ahora revisa si hay peticiones de botones MIENTRAS transmite
void enviarStream(WiFiClient &streamClient) {
  streamClient.println("HTTP/1.1 200 OK");
  streamClient.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  streamClient.println();

  while (streamClient.connected()) {
    // 1. Obtener foto
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Fallo captura de cámara");
      continue;
    }

    // 2. Enviar cabecera y datos de la foto
    streamClient.println("--frame");
    streamClient.println("Content-Type: image/jpeg");
    streamClient.printf("Content-Length: %d\r\n\r\n", fb->len);
    streamClient.write(fb->buf, fb->len);
    streamClient.println();
    
    // Liberar memoria de la foto
    esp_camera_fb_return(fb);

    // -------------------------------------------------------
    // Mientras estamos en el bucle de video, revisamos si 
    // hay OTRO cliente intentando conectarse (el botón)
    WiFiClient commandClient = server.available();
    
    if (commandClient) {
      // Si hay alguien tocando la puerta (botón), leemos qué quiere
      String req = commandClient.readStringUntil('\r');
      commandClient.flush();
      
      // Ejecutamos la acción (LED ON/OFF)
      ejecutarComando(req);
      
      // Le respondemos rápido para cerrar esa conexión secundaria
      commandClient.println("HTTP/1.1 200 OK");
      commandClient.println("Connection: close");
      commandClient.println();
      commandClient.stop();
    }
    // -------------------------------------------------------
  }
}

// ---------------------------------------------------
void loop() {
  WiFiClient client = server.available();
  
  if (client) {
    String req = client.readStringUntil('\r');
    client.flush();

    // Si piden el video, entramos en el bucle de streaming
    if (req.indexOf("GET /stream") >= 0) {
      enviarStream(client);
    }
    // Si es una petición normal (página principal o led si no hay video activo)
    else {
      // Revisamos si es comando de LED (por si acaso el video no estaba activo)
      ejecutarComando(req);

      if (req.indexOf("GET / ") >= 0) {
        // Enviar Página Web
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println();

        client.println("<html><head>");
        client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
        client.println("<style>");
        client.println("body { text-align:center; font-family: Arial; background-color: #f2f2f2; }");
        client.println("h2 { color: #333; }");
        client.println("button { padding: 15px 30px; font-size: 18px; border: none; border-radius: 5px; cursor: pointer; margin: 10px; }");
        client.println(".btn-on { background-color: #28a745; color: white; }");
        client.println(".btn-off { background-color: #dc3545; color: white; }");
        client.println("</style>");
        client.println("</head><body>");

        client.println("<h2>ESP32-CAM Stream & Control</h2>");
        
        // Imagen del stream
        client.println("<img src='/stream' style='width:320px; border-radius:10px;'>");
        client.println("<br><br>");

        // Botones usando fetch simple (sin recargar página)
        client.println("<button class='btn-on' onclick=\"fetch('/led/on')\">ENCENDER LED</button>");
        client.println("<button class='btn-off' onclick=\"fetch('/led/off')\">APAGAR LED</button>");

        client.println("</body></html>");
      }
      else {
        // Respuesta genérica para los botones
        client.println("HTTP/1.1 200 OK");
        client.println("Connection: close");
        client.println();
      }
      client.stop();
    }
  }
}
