
/*
  ESP8266 MQTT TV control program
  It connects to an MQTT server
  It will reconnect to the server if the connection is lost
  Written by W. Hoogervorst
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <credentials.h> //MQTT and WiFi credentials
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLib.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>

#define ONE_WIRE_BUS 2  // Temp sensor on GPIO2 of ESP8266
#define TEMPERATURE_PRECISION 10 // 18B20 temperature sensor precision
// define times
#define MINUTE 60  // 60 seconds = 1 minute
#define TEMPDELAY MINUTE

OneWire oneWire(ONE_WIRE_BUS);        // start onewire for 18B20
DallasTemperature sensors(&oneWire);
DeviceAddress Sensor1;

float TempC;

uint32_t measurement = 0;
long lastReconnectAttempt, lastBlink = 0;

String tmp_str; // String for publishing the int's as a string to MQTT
char buf[5];

// for HTTPupdate
const char* host = "SonyTVControl";
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
const char* software_version = "version 1";

/*credentials & definitions */
//MQTT
const char* mqtt_id = "SonyTVControl";
const char* power_topic = "SonyTV/power";
const char* volumeup_topic = "SonyTV/volup";
const char* volumedown_topic = "SonyTV/voldown";

IRsend irsend(3); //an IR led is connected to GPIO3 (RX pin, no serial communication possible then)

#define SERIALDEBUG 0

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  irsend.begin();

  WiFi.mode(WIFI_STA);
  setup_wifi();

  // for HTTPudate
  MDNS.begin(host);
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  MDNS.addService("http", "tcp", 80);

  httpServer.on("/", handleRoot);
  httpServer.on("/power", handle_power);
  httpServer.on("/volup", handle_volup);
  httpServer.on("/voldown", handle_voldown);
  httpServer.onNotFound(handle_NotFound);

  // init of sensors
  sensors.begin();
  sensors.setResolution(Sensor1, TEMPERATURE_PRECISION);

  // MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  if (!client.connected()) {
    reconnect();
  }
}

void loop() {
  if (client.connected())
  {
    // Client connected
    client.loop();
    httpServer.handleClient();    // for HTTPupdate

    if (now() > measurement + TEMPDELAY)
    {
      handleTemp();
      measurement = now();
    }
  }
  else
    // Client is not connected
  {
    long now = millis();
    long now2 = millis();
    if (now2 - lastBlink > 500) {          // blink the LED while not connected to MQTT
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      lastBlink = now2;
    }

    if (now - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
}


void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  WiFi.begin(mySSID, myPASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    digitalWrite(LEDPIN, HIGH);
  }
}

void callback(char* topic, byte * payload, unsigned int length) {

  // Switch on the LED if an 1 was received as first character

  if ((char)topic[7] == 'p')      // check for the "p" of SonyTV/power
  {
    //irsend.sendSony(0x290, 12, 3); // mute sound
    irsend.sendSony(0xA90, 12, 3);
  }
  if ((char)topic[10] == 'u')    // check for the "u" of SonyTV/volup
  {
    irsend.sendSony(0x490, 12, 3);
    delay(100);
    irsend.sendSony(0x490, 12, 3);
    delay(100);
    irsend.sendSony(0x490, 12, 3);
  }
  if ((char)topic[10] == 'd')    // check for the "u" of SonyTV/volup
  {
    irsend.sendSony(0xc90, 12, 3);
    delay(100);
    irsend.sendSony(0xc90, 12, 3);
    delay(100);
    irsend.sendSony(0xc90, 12, 3);
  }
}

boolean reconnect()
{
  if (WiFi.status() != WL_CONNECTED) {    // check if WiFi connection is present
    setup_wifi();
  }
  if (client.connect(mqtt_id)) {
    // ... and resubscribe
    client.subscribe(power_topic);
    client.subscribe(volumeup_topic);
    client.subscribe(volumedown_topic);
  }
  return client.connected();
}

void handleRoot() {
  httpServer.send(200, "text/html", SendHTML());
  /*
    String message = "Sonoff WimIOT\nDevice: ";
    message += mqtt_id;
    message += "\nSoftware version: ";
    message += software_version;
    message += "\nUpdatepath at http://[IP]/update";
    httpServer.send(200, "text/plain", message);
  */
}
void handle_OnConnect() {
  httpServer.send(200, "text/html", SendHTML());
}

void handle_power() {
  irsend.sendSony(0xA90, 12, 3);
  httpServer.send(200, "text/html", SendHTML());
}

void handle_volup() {
  irsend.sendSony(0x490, 12, 3);
  httpServer.send(200, "text/html", SendHTML());
}

void handle_voldown() {
  irsend.sendSony(0xc90, 12, 3);
  httpServer.send(200, "text/html", SendHTML());
}

void handle_NotFound() {
  httpServer.send(404, "text/plain", "Not found");
}

String SendHTML() {
  handleTemp();
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>";
  ptr += mqtt_id;
  ptr += "</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 25px auto 30px;} h3 {color: #444444;margin-bottom: 30px;}\n";
  ptr += ".button {width: 150px;background-color: #1abc9c;border: none;color: white;padding: 13px 10px;text-decoration: none;font-size: 20px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-vol {background-color: #1abc9c;}\n";
  ptr += ".button-vol:active {background-color: #16a085;}\n";
  ptr += ".button-pwr {background-color: #34495e;}\n";
  ptr += ".button-pwr:active {background-color: #2c3e50;}\n";
  ptr += ".button-update {background-color: #a32267;}\n";
  ptr += ".button-update:active {background-color: #961f5f;}\n";
  ptr += "p {font-size: 18px;color: #383535;margin-bottom: 15px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>TV Web Control</h1>\n";
  ptr += "<h3>Control of Power and Volume and link to HTTPWebUpdate</h3>\n";
  ptr += "<p>WimIOT\nDevice: ";
  ptr += mqtt_id;
  ptr += "<br>Software version: ";
  ptr += software_version;
  ptr += "<br><br></p>";

  ptr += "<a class=\"button button-pwr\" href=\"/power\">Power</a><br><br><br><br>\n";
  ptr += "<a class=\"button button-vol\" href=\"/voldown\">Vol -</a>&nbsp;&nbsp;\n";
  ptr += "<a class=\"button button-vol\" href=\"/volup\">Vol +</a><br><br><br>\n";
  ptr += "<p>Temperature: ";
  ptr += TempC;
  ptr += "<br><br></p>\n";
  ptr += "<p>Click for update page</p><a class =\"button button-update\" href=\"/update\">Update</a>\n";

  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}

void handleTemp(void)
{
  sensors.requestTemperatures();
  TempC = sensors.getTempCByIndex(0);
  int temp2 = round(TempC * 10);
  TempC = temp2 / (float)10;
  tmp_str = String(TempC); //converting WiFI.RSSI to a string
  tmp_str.toCharArray(buf, tmp_str.length() + 1);
  client.publish("sensor/temperature4", buf);
}
