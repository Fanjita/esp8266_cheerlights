#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// *****************************************************
// THESE MUST BE CONFIGURED!
// *****************************************************
const char* ssid = "...";
const char* password = "...";

const char* mqtt_server = "test.mosquitto.org";
const char* INTOPIC = "yhsCheerlights/inTopic";
const char* OUTTOPIC = "yhsCheerlights/outTopic";

void setupOTA() {
  Serial.begin(115200);
  Serial.println("\nBooting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("cheerlights");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

WiFiClient espClient;
PubSubClient client(espClient);
const int MAX_MSG_LEN = 50;
const int MAX_MSG_PARAMS = 10;
long lastMsg = 0;
char msg[MAX_MSG_LEN+1];
int value = 0;


const byte redPin = 15;
const byte greenPin = 12;
const byte bluePin = 13;

void setup()
{
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTT_recv_callback);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  setupOTA();
}

// Convert hex char into 0-15 value
byte hexchar2byte(char hexchar)
{
  if ((hexchar <= '9') && (hexchar >= '0'))
  {
    return hexchar - '0';
  }

  // Ensure lower case
  hexchar = hexchar | 0x20;

  if ((hexchar >= 'a') && (hexchar <= 'f'))
  {
    return 10 + hexchar - 'a';
  }

  // Bogus character. Safest to return 0.
  Serial.println("Bad char");
  return 0;
}

// Convert 2 char hex string to byte value
byte hex2byte(const char* hexstr)
{
  byte val1 = hexchar2byte(hexstr[0]);
  byte val2 = hexchar2byte(hexstr[1]);
  byte val = (val1 << 4) + val2;
  return val;
}

bool ProcessCommand(char *msg)
{
  bool recognised = false;
  int msgLen = strlen(msg);

  // Just assume a straight #RRGGBB value.
  if ((msg[0] == '#') && (strlen(msg) == 7))
  {
    int r = hex2byte(msg + 1);
    int g = hex2byte(msg + 3);
    int b = hex2byte(msg + 5);

    analogWrite(redPin, r);
    analogWrite(greenPin, g);
    analogWrite(bluePin, b);

    sprintf(msg, "setting RGB: %d,%d,%d", r,g,b);
    client.publish(OUTTOPIC, (char*)msg);

    recognised = true;
  }
  else
  {
    Serial.print("BAD MSG - IGNORED : '");
    Serial.print(msg);
    Serial.println("'");
  }

  return recognised;
}

void MQTT_recv_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((length + 1) >= MAX_MSG_LEN)
  {
    Serial.println("Msg too long - ignored");
    return;
  }

  strncpy(msg, (char*)payload, length);
  msg[length] = 0;

  bool recognised = ProcessCommand(msg);
  const char *response = recognised ? "OK" : "UNKNOWN CMD";

  Serial.println(response);

  client.publish(OUTTOPIC, (char*)response);
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(OUTTOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(INTOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop()
{
  if (!client.connected()) {
    reconnectMQTT();
  }

  // Do regular processing for the pubsub client and OTA
  client.loop();

  ArduinoOTA.handle();

  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    ++value;
    snprintf (msg, MAX_MSG_LEN, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(OUTTOPIC, msg);
  }

  yield();
}
