#include "PubSubClient.h"
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>
#include <FS.h>         

const char *wifi_config_name = "Base Configuration";
int port = 80;
char passcode[40] = "";
char host_name[40] = "";
char last_codes[5][20];
char last_code_idx = 0;

//Ports
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15

#define PIN_LED1 D0
#define PIN_LED0 D4

#define PIN_IROUT0 D6
#define PIN_IROUT1 D7
#define PIN_IRIN0 D5

//Out
#define PIN_OUT0 D4

//In
#define PIN_IN0 D0
#define PIN_IN1 D1
#define PIN_IN2 D2
#define PIN_IN3 D8
#define PIN_RESET 10

//MQTT
#define TOPICO_SUBSCRIBE_D4  "MQTTD4Envia20170506"
#define TOPICO_PUBLISH_D4  "MQTTD4Recebe20170506"

#define TOPICO_PUBLISH_IN0  "MQTTD0Recebe20170506"
#define TOPICO_PUBLISH_IN1  "MQTTD1Recebe20170506"
#define TOPICO_PUBLISH_IN2  "MQTTD2Recebe20170506"
#define TOPICO_PUBLISH_IN3  "MQTTD3Recebe20170506"

#define ID_MQTT "HomeAut"

const int configpin = PIN_RESET;

const char* BROKER_MQTT = "192.168.1.101";
int BROKER_PORT = 1883;

//WIFI
WiFiClient espClient;
PubSubClient MQTT(espClient);
char StateOutput = '0';

ESP8266WebServer server(port);
HTTPClient http;
Ticker ticker;

bool shouldSaveConfig = false;              // Flag for saving data

//IR
IRrecv irrecv(PIN_IRIN0);                   // Receiving pin
IRsend irsend1(PIN_IROUT0);                 // Transmitting preset 1
IRsend irsend2(PIN_IROUT1);                 // Transmitting preset 2

//=============================================================================
// MQTT
//=============================================================================
void initMQTT() 
{
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}


void mqtt_callback(char* topic, byte* payload, unsigned int length) 
{
 String msg;

 for(int i = 0; i < length; i++)
 {
  char c = (char)payload[i];
  msg += c;
 }

 if(msg.equals("L"))
 {
  digitalWrite(PIN_LED0,LOW);
  StateOutput = '1';
 }

 if (msg.equals("D"))
 {
  digitalWrite(PIN_LED0, HIGH);
  StateOutput = '0';  
 }
 
}

void reconnectMQTT() 
{
  while (!MQTT.connected())
  {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if(MQTT.connect(ID_MQTT))
    {
      Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPICO_SUBSCRIBE_D4);
    }
    else
    {
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Havera nova tentatica de conexao em 2s");
      delay(2000);
    }
  }
}


void CheckMQTT(void)
{
  if(!MQTT.connected())
    reconnectMQTT();
}

void OutputMQTT(void)
{

  Serial.print("OUT D4=");
  Serial.print(StateOutput);
  Serial.println();

  if (StateOutput == '0')
    MQTT.publish(TOPICO_PUBLISH_D4, "D");
  if (StateOutput == '1')
    MQTT.publish(TOPICO_PUBLISH_D4, "L");

  int res = 0;
  res = digitalRead(PIN_IN0);
  Serial.print("IN D0=");
  Serial.print(res);
  Serial.println();

  if (res == 0)
    MQTT.publish(TOPICO_PUBLISH_IN0, "D");
  if (res == 1)
    MQTT.publish(TOPICO_PUBLISH_IN0, "L");  
 
  res = digitalRead(PIN_IN1);
  Serial.print("IN D1=");
  Serial.print(res);
  Serial.println();

  SendGet(BROKER_MQTT, "sensor/?D1=" + String(res, DEC));

  if (res == 0)
    MQTT.publish(TOPICO_PUBLISH_IN1, "D");
  if (res == 1)
    MQTT.publish(TOPICO_PUBLISH_IN1, "L");   

  res = digitalRead(PIN_IN2);
  Serial.print("IN D2=");
  Serial.print(res);
  Serial.println();

  if (res == 0)
    MQTT.publish(TOPICO_PUBLISH_IN2, "D");
  if (res == 1)
    MQTT.publish(TOPICO_PUBLISH_IN2, "L");  

  res = digitalRead(PIN_IN3);
  Serial.print("IN D8=");
  Serial.print(res);
  Serial.println();

  if (res == 0)
    MQTT.publish(TOPICO_PUBLISH_IN3, "D");
  if (res == 1)
    MQTT.publish(TOPICO_PUBLISH_IN3, "L");  
 
  delay(1000);
}

//=============================================================================
// Configuration Outputs
//=============================================================================
void initOutput(void)
{
  pinMode(PIN_LED0, OUTPUT);
  digitalWrite(PIN_LED0, HIGH);

  pinMode(PIN_IN0, INPUT);
  pinMode(PIN_IN1, INPUT);
  pinMode(PIN_IN2, INPUT);
  pinMode(PIN_IN3, INPUT);
}


//=============================================================================
// Callback Save config
//=============================================================================
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


//=============================================================================
// Toggle state
//=============================================================================
void tick()
{
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
}

void disableLed()
{
  Serial.println("Turning off the LED to save power.");
  ticker.detach();              // Stopping the ticker
}

//==============================================================================
// WiFiManager configuration
//=============================================================================
void ConfigModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

bool setupWifi(bool resetConf) {
  
  // set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // reset settings - for testing
  if (resetConf)
    wifiManager.resetSettings();

  // set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(ConfigModeCallback);
  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  char port_str[40] = "80";

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strncpy(host_name, json["hostname"], 40);
          strncpy(passcode, json["passcode"], 40);
          strncpy(port_str, json["port_str"], 40);
          port = atoi(json["port_str"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  // end read

  WiFiManagerParameter custom_hostname("hostname", "Choose a hostname to this IRBlaster", host_name, 40);
  wifiManager.addParameter(&custom_hostname);
  WiFiManagerParameter custom_passcode("passcode", "Choose a passcode", passcode, 40);
  wifiManager.addParameter(&custom_passcode);
  WiFiManagerParameter custom_port("port_str", "Choose a port", port_str, 40);
  wifiManager.addParameter(&custom_port);

  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(wifi_config_name)) {
    Serial.println("failed to connect and hit timeout");
    // reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  // if you get here you have connected to the WiFi
  strncpy(host_name, custom_hostname.getValue(), 40);
  strncpy(passcode, custom_passcode.getValue(), 40);
  strncpy(port_str, custom_port.getValue(), 40);
  port = atoi(port_str);

  if (port != 80) {
    Serial.println("Default port changed");
    server = ESP8266WebServer(port);
  }

  Serial.println("WiFi connected! User chose hostname '" + String(host_name) + String("' passcode '") + String(passcode) + "' and port '" + String(port_str) + "'");

  // save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println(" config...");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["hostname"] = host_name;
    json["passcode"] = passcode;
    json["port_str"] = port_str;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println("");
    json.printTo(configFile);
    configFile.close();
    //e nd save
  }
  ticker.detach();

  // keep LED on
  //digitalWrite(BUILTIN_LED, LOW);
  return true;
}


//==============================================================================
// Setup
//=============================================================================
void setup() {

  // Initialize serial
  Serial.begin(115200);
  Serial.println("");
  Serial.println("ESP8266 IR Controller");
  pinMode(configpin, INPUT_PULLUP);
  if (!setupWifi(digitalRead(configpin) == LOW))
    return;

  WiFi.hostname(host_name);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  wifi_set_sleep_type(LIGHT_SLEEP_T);
  //digitalWrite(BUILTIN_LED, LOW);
  // Turn off the led in 5s
  ticker.attach(5, disableLed);

  // Configure mDNS
  if (MDNS.begin(host_name)) {
    Serial.println("mDNS started. Hostname is set to " + String(host_name) + ".local");
  }
  MDNS.addService("http", "tcp", port); // Anounce the ESP as an HTTP service
  String port_str((port == 80)? String("") : String(port));
  Serial.println("URL to send commands: http://" + String(host_name) + ".local:" + port_str);

  // Configure the server
  server.on("/json", []() { // JSON handler for more complicated IR blaster routines
    Serial.println("Connection received - JSON");

    DynamicJsonBuffer jsonBuffer;
    JsonArray& root = jsonBuffer.parseArray(server.arg("plain"));

    if (!root.success()) {
      Serial.println("JSON parsing failed");
      server.send(400, "text/html", "Failed");
    } else if (server.arg("pass") != passcode) {
      Serial.println("Unauthorized access");
      server.send(401, "text/html", "Unauthorized");
    } else {
      server.send(200, "text/json", "Valid JSON object received, sending sequence");
      for (int x = 0; x < root.size(); x++) {
        String type = root[x]["type"];
        String ip = root[x]["ip"];
        int rdelay = root[x]["rdelay"];
        int pulse = root[x]["pulse"];
        int pdelay = root[x]["pdelay"];
        int repeat = root[x]["repeat"];
        int out = root[x]["out"];

        if (pulse == 0) pulse = 1; // Make sure pulse isn't 0
        if (repeat == 0) repeat = 1; // Make sure repeat isn't 0
        if (pdelay == 0) pdelay = 100; // Default pdelay
        if (rdelay == 0) rdelay = 1000; // Default rdelay

        if (type == "delay") {
          delay(rdelay);
        } else if (type == "raw") {
          JsonArray &raw = root[x]["data"]; // Array of unsigned int values for the raw signal
          int khz = root[x]["khz"];
          if (khz == 0) khz = 38; // Default to 38khz if not set
          rawblast(raw, khz, rdelay, pulse, pdelay, repeat, pickIRsend(out));
        } else if (type == "roku") {
          String data = root[x]["data"];
          SendPost(ip, data);
        } else {
          String data = root[x]["data"];
          long address = root[x]["address"];
          int len = root[x]["length"];
          irblast(type, data, len, rdelay, pulse, pdelay, repeat, address, pickIRsend(out));
        }
      }
    }
  });

  // Setup simple msg server to mirror version 1.0 functionality
  server.on("/msg", []() {
    Serial.println("Connection received - MSG");
    if (server.arg("pass") != passcode) {
      Serial.println("Unauthorized access");
      server.send(401, "text/html", "Unauthorized");
    } else {
      String type = server.arg("type");
      String data = server.arg("data");
      String ip = server.arg("ip");
      int len = server.arg("length").toInt();
      long address = (server.hasArg("address")) ? server.arg("address").toInt() : 0;
      int rdelay = (server.hasArg("delay")) ? server.arg("rdelay").toInt() : 1000;
      int pulse = (server.hasArg("pulse")) ? server.arg("pulse").toInt() : 1;
      int pdelay = (server.hasArg("pdelay")) ? server.arg("pdelay").toInt() : 100;
      int repeat = (server.hasArg("repeat")) ? server.arg("repeat").toInt() : 1;
      int out = (server.hasArg("out")) ? server.arg("out").toInt() : 0;
      if (server.hasArg("code")) {
        String code = server.arg("code");
        char separator = ':';
        data = getValue(code, separator, 0);
        type = getValue(code, separator, 1);
        len = getValue(code, separator, 2).toInt();
      }

      if (type == "roku") {
        SendPost(ip, data);
      } else {
        irblast(type, data, len, rdelay, pulse, pdelay, repeat, address, pickIRsend(out));
      }
      server.send(200, "text/html", "code sent");
    }
  });

  server.on("/", []() {
    Serial.println("Connection received");
    server.send(200, "text/html", "Server is running");
  });

  server.begin();
  Serial.println("HTTP Server started on port " + String(port));

  irsend1.begin();
  irsend2.begin();
  irrecv.enableIRIn();
  Serial.println("Ready to send and receive IR signals");

  initOutput();
  initMQTT();
}


//==============================================================================
// Send post/get
//==============================================================================
int SendPost(String ip, String data) {
  String url = "http://" + ip + ":8060/" + data;
  http.begin(url);
  Serial.println(url);
  Serial.println("Sending roku command");
  return http.POST("");
  http.end();
}

int SendGet(String ip, String data) {
  String url = "http://" + ip + ":8060/" + data;
  http.begin(url);
  Serial.println(url);
  return http.GET();
  http.end();
}

//==============================================================================
// IR
//==============================================================================
void  ircode (decode_results *results)
{
  // Panasonic has an Address
  if (results->decode_type == PANASONIC) {
    //Serial.print(results->panasonicAddress, HEX);
    Serial.print(":");
  }

  // Print Code
  //Serial.print(results->value, HEX);
  serialPrintUint64(results->value, HEX);
}

IRsend pickIRsend (int out) {
  switch (out) {
    case 1: return irsend1;
    case 2: return irsend2;
    default: return irsend1;
  }
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void  encoding (decode_results *results)
{
  switch (results->decode_type) {
    default:
    case UNKNOWN:      Serial.print("UNKNOWN");       break;
    case NEC:          Serial.print("NEC");           break;
    case NEC_LIKE:     Serial.print("NEC (non-strict)");  break;
    case SONY:         Serial.print("SONY");          break;
    case RC5:          Serial.print("RC5");           break;
    case RC5X:         Serial.print("RC5X");          break;
    case RC6:          Serial.print("RC6");           break;
    case RCMM:         Serial.print("RCMM");          break;
    case DISH:         Serial.print("DISH");          break;
    case SHARP:        Serial.print("SHARP");         break;
    case JVC:          Serial.print("JVC");           break;
    case SANYO:        Serial.print("SANYO");         break;
    case SANYO_LC7461: Serial.print("SANYO_LC7461");  break;
    case MITSUBISHI:   Serial.print("MITSUBISHI");    break;
    case SAMSUNG:      Serial.print("SAMSUNG");       break;
    case LG:           Serial.print("LG");            break;
    case WHYNTER:      Serial.print("WHYNTER");       break;
    case AIWA_RC_T501: Serial.print("AIWA_RC_T501");  break;
    case PANASONIC:    Serial.print("PANASONIC");     break;
    case DENON:        Serial.print("DENON");         break;
    case COOLIX:       Serial.print("COOLIX");        break;
    case NIKAI:        Serial.print("NIKAI");         break;
  }
}

void fullCode (decode_results *results)
{
  Serial.print("One line: ");
  //Serial.print(results->value, HEX);
  serialPrintUint64(results->value, HEX);
  Serial.print(":");
  encoding(results);
  Serial.print(":");
  Serial.print(results->bits, DEC);
  Serial.println("");
}

void dumpInfo (decode_results *results)
{
  // Show Encoding standard
  Serial.print("Encoding  : ");
  encoding(results);
  Serial.println("");

  // Show Code & length
  Serial.print("Code      : ");
  ircode(results);
  Serial.print(" (");
  Serial.print(results->bits, DEC);
  Serial.println(" bits)");
}

void  dumpRaw (decode_results *results)
{
  // Print Raw data
  Serial.print("Timing[");
  Serial.print(results->rawlen - 1, DEC);
  Serial.println("]: ");

  for (int i = 1;  i < results->rawlen;  i++) {
    unsigned long  x = results->rawbuf[i] * RAWTICK;
    if (!(i & 1)) {  // even
      Serial.print("-");
      if (x < 1000)  Serial.print(" ") ;
      if (x < 100)   Serial.print(" ") ;
      Serial.print(x, DEC);
    } else {  // odd
      Serial.print("     ");
      Serial.print("+");
      if (x < 1000)  Serial.print(" ") ;
      if (x < 100)   Serial.print(" ") ;
      Serial.print(x, DEC);
      if (i < results->rawlen - 1) Serial.print(", "); //',' not needed for last one
    }
    if (!(i % 8))  Serial.println("");
  }
  Serial.println("");                    // Newline
}

void  dumpCode (decode_results *results)
{
  // Start declaration
  Serial.println("Raw (code):");
  Serial.print("unsigned int  ");          // variable type
  Serial.print("rawData[");                // array name
  Serial.print(results->rawlen - 1, DEC);  // array size
  Serial.print("] = {");                   // Start declaration

  // Dump data
  for (int i = 1;  i < results->rawlen;  i++) {
    Serial.print(results->rawbuf[i] * RAWTICK, DEC);
    if ( i < results->rawlen - 1 ) Serial.print(","); // ',' not needed on last one
    if (!(i & 1))  Serial.print(" ");
  }

  // End declaration
  Serial.print("};");  //

  // Comment
  Serial.print("  // ");
  encoding(results);
  Serial.print(" ");
  ircode(results);

  // Newline
  Serial.println("");

  // Now dump "known" codes
  if (results->decode_type != UNKNOWN) {

    // Some protocols have an address
    if (results->decode_type == PANASONIC) {
      Serial.print("unsigned int  addr = 0x");
      //Serial.print(results->panasonicAddress, HEX);
      Serial.println(";");
    }

    // All protocols have data
    Serial.print("unsigned int  data = ");
    //Serial.print(results->value, HEX);
    serialPrintUint64(results->value, HEX);
    Serial.println(";");
  }
}

unsigned long HexToLongInt(String h)
{
  // this function replace the strtol as this function is not able to handle hex numbers greather than 7fffffff
  // I'll take char by char converting from hex to char then shifting 4 bits at the time
  int i;
  unsigned long tmp = 0;
  unsigned char c;
  int s = 0;
  h.toUpperCase();
  for (i = h.length() - 1; i >= 0 ; i--)
  {
    // take the char starting from the right
    c = h[i];
    // convert from hex to int
    c = c - '0';
    if (c > 9)
      c = c - 7;
    // add and shift of 4 bits per each char
    tmp += c << s;
    s += 4;
  }
  return tmp;
}

void irblast(String type, String dataStr, int len, int rdelay, int pulse, int pdelay, int repeat, long address, IRsend irsend) {
  Serial.println("Blasting off");
  type.toLowerCase();
  long data = HexToLongInt(dataStr);
  // Repeat Loop
  for (int r = 0; r < repeat; r++) {
    // Pulse Loop
    for (int p = 0; p < pulse; p++) {
      Serial.print(data, HEX);
      Serial.print(":");
      Serial.print(type);
      Serial.print(":");
      Serial.println(len);
      if (type == "nec") {
        irsend.sendNEC(data, len);
      } else if (type == "sony") {
        irsend.sendSony(data, len);
      } else if (type == "coolix") {
        irsend.sendCOOLIX(data, len);
      } else if (type == "whynter") {
        irsend.sendWhynter(data, len);
      } else if (type == "panasonic") {
        Serial.println(address);
        irsend.sendPanasonic(address, data);
      } else if (type == "jvc") {
        irsend.sendJVC(data, len, 0);
      } else if (type == "samsung") {
        irsend.sendSAMSUNG(data, len);
      } else if (type == "sharp") {
        irsend.sendSharpRaw(data, len);
      } else if (type == "dish") {
        irsend.sendDISH(data, len);
      } else if (type == "rc5") {
        irsend.sendRC5(data, len);
      } else if (type == "rc6") {
        irsend.sendRC6(data, len);
      } else if (type == "lg") {
        irsend.sendLG(data, len);
      }
      delay(pdelay);
    }
    delay(rdelay);
  }
}

void rawblast(JsonArray &raw, int khz, int rdelay, int pulse, int pdelay, int repeat, IRsend irsend) {
  Serial.println("Raw transmit");

  // Repeat Loop
  for (int r = 0; r < repeat; r++) {
    // Pulse Loop
    for (int p = 0; p < pulse; p++) {
      Serial.println("Sending code");
      irsend.enableIROut(khz);
      int first = raw[0];
      for (unsigned int i = 0; i < raw.size(); i++) {
        unsigned int val = raw[i];
        if (i & 1) irsend.space(val);
        else       irsend.mark(val);
      }
      irsend.space(0);
      delay(pdelay);
    }
    delay(rdelay);
  }
}

void loop() {
  server.handleClient();
  decode_results  results;        // Somewhere to store the results

  if (irrecv.decode(&results)) {  // Grab an IR code
    Serial.println("Signal received:");
     fullCode(&results);           // Print the singleline value
    //dumpInfo(&results);           // Output the results
    //dumpRaw(&results);            // Output the results in RAW format
    dumpCode(&results);           // Output the results as source code
    Serial.println("");           // Blank line between entries
    strncpy(last_codes[last_code_idx], "", 20);
    last_code_idx = (last_code_idx + 1) % 5;
    irrecv.resume();              // Prepare for the next value
  }

  //CheckMQTT();
  //OutputMQTT();
  //MQTT.loop();
  
  delay(200);
}

