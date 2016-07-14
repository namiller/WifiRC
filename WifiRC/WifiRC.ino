#include <DNSServer.h>

#include <ESP8266WiFi.h>
#include "FS.h"
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <math.h>

// Interface for New Bright RC car
// (http://www.et.byu.edu/~bmazzeo/LTR/tech.phtml)
#define LEFT_PIN 12
#define RIGHT_PIN 13
#define UP_PIN 14
#define DOWN_PIN 15

/* Set these to your desired credentials. */
const char *ssid = "RC car";

IPAddress apIP(192, 168, 1, 1);

const byte DNS_PORT = 53;
DNSServer dnsServer;

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
void sendCommand(byte speed, int angle);

void sendXY(int x, int y) {
  Serial.printf("sending xy (%i, %i)\n", x, y);
  bool act = (x*x+y*y) > 1000;
  int angle = atan2(x,-1*y) * 180.0 / M_PI ;
  sendCommand(act, angle);
}

bool parse(char *s, int *x, int *y) {
  char *strt = strchr(s, '(');
  if (strt == 0) 
  return false;
  *x = atoi(strt+1);
  strt = strchr(s, ',');
  if (strt == 0)
  return false;
  *y = atoi(strt+1);
  return true;
}

void sendCommand(byte speed, int angle) {
  Serial.printf("sending command (%i, %i)\n", (int)speed, angle);
  angle = (angle + 720) % 360;
  bool fb_;
  if (angle < 90 || angle > 270) {
    fb_ = true;
  } else {
    fb_ = false;
  }
  bool lr_;
  if (angle < 180) {
    lr_ = false;
  } else {
    lr_ = true;
  }
  bool fb_en = false;
  bool lr_en = false;
  if (angle < 60 || angle > 300 || (angle > 120 && angle < 240)) {
    fb_en = true;
  }
  if ( (angle > 30 && angle < 150) || (angle > 210 && angle < 330)) {
    lr_en = true;
  }
  
  digitalWrite(UP_PIN, fb_ * speed * fb_en);
  digitalWrite(DOWN_PIN, (!fb_) * speed * fb_en);
  digitalWrite(LEFT_PIN, lr_ * speed * lr_en);
  digitalWrite(RIGHT_PIN, (!lr_) * speed * lr_en);
}


/* Just a little test message.  Go to http://192.168.4.1 in a web browser
 * connected to this access point to see it.
 */
void handleRoot() {
  File f = SPIFFS.open("/control.html", "r");
  server.send(200, "text/html", f.readString());
  f.close();
}

void handleJs() {
  File f = SPIFFS.open("/joystick.js", "r");
  server.send(200, "application/javascript", f.readString());
  f.close();
}

void handleWebSocket(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch(type) {
      case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\n", num);
        sendCommand(0,0);
      break;
      case WStype_CONNECTED: {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
        break;
        case WStype_TEXT:
          Serial.printf("[%u] get Text: %s\n", num, payload);
          int x, y;
          if (parse((char*)payload, &x, &y)) {
            sendXY(x,y);
          } else {
            sendCommand(0,0);
          }
        break;
     }
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid);

  Serial.println("Configuring DNS");
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(DNS_PORT, "*", apIP);

  Serial.println("initializing FS");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  
  Serial.println("Configuring http server");
  server.on("/", handleRoot);
  server.on("/joystick.js", handleJs);
  server.begin();
  
  Serial.println("Configuring ws server");
  webSocket.begin();
  webSocket.onEvent(handleWebSocket);

  Serial.println("Initializing motors");
  
  pinMode(UP_PIN, OUTPUT);
  pinMode(RIGHT_PIN, OUTPUT);
  pinMode(DOWN_PIN, OUTPUT);
  pinMode(LEFT_PIN, OUTPUT);
  sendCommand(0,0);
  
  Serial.println("Configuration complete");
}

void loop() {
  dnsServer.processNextRequest();
  webSocket.loop();
  server.handleClient();
}
