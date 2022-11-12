#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include <string.h>

#define LED_PIN 2

////////////// Network constant /////////////
#define CONNECTING_THRESHOLD 20
#define THINGSPEAK_PORT 80

const char *SSID = "Minh Cu";
const char *PASSWORD = "12345678";
const char *THINGSPEAK_WRITE_KEY = "8YEPB0J78C2PFPR3";
const uint32_t THINGSPEAK_CHANNEL_ID = 1928318;
WiFiClient client;
////////// End of Network constant //////////

// AC712-5A Sensor's constains
// unit: V, A, W

#define AC712_ANALOG_IN A0
#define VOLTAGE_OFFSET 24
#define AMPERAGE_SENSITIVITY (66.0f / 1000.0f)
#define VOLTAGE_PER_POINT (5.0f / 1024.0f)
#define POWER_USAGE_THRESHOLD 5
float power_timer;
float total_power_usage;
////////// End of AC712 //////////

// YF-S201 Sensor's constains
// unit: ml
#define YFS201_DATA_IN D2
#define FLOW_SENSITIVITY 355.0f / 175.0f
#define WATER_USAGE_THRESHOLD 1000
volatile long pulse;

float total_volume;
float water_timer;
////////// End of YF-S201 //////////

uint64_t global_timer = 0;

// unit: V
float calculate_voltage() {
  int counts = analogRead(AC712_ANALOG_IN) - VOLTAGE_OFFSET;
  float voltage = counts * VOLTAGE_PER_POINT;

  return voltage - 2.5f;
}

// unit: A
float calculate_amp() {
  float voltage = calculate_voltage();
  return voltage / AMPERAGE_SENSITIVITY;
}

// unit: W/s
float calculate_power_usage() {
  float usage = calculate_amp() * calculate_voltage();
  if (millis() - power_timer > 1000) {
    power_timer = millis();
    total_power_usage += usage;
  }
  return usage;
}

void IRAM_ATTR increase() {
  pulse++;
}

// unit ml/s
float calculate_flow_rate() {
  float flow_rate = FLOW_SENSITIVITY * pulse;
  if (millis() - water_timer > 1000) {
    pulse = 0;
    water_timer = millis();
    total_volume += flow_rate;
  }

  return flow_rate;
}

void led_blink() {
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
}

void check_if_water_usage_overflow() {
  if (total_volume >= WATER_USAGE_THRESHOLD) {
    led_blink();
    Serial.print("Total water usage has exceed `");
    Serial.print(WATER_USAGE_THRESHOLD);
    Serial.println("ml`");
  } else digitalWrite(LED_PIN, LOW);
}

void check_if_power_usage_overflow() {
  if (total_power_usage >= POWER_USAGE_THRESHOLD) {
    led_blink();
    Serial.print("Total power usage has exceed `");
    Serial.print(POWER_USAGE_THRESHOLD);
    Serial.println("W`");
  } else digitalWrite(LED_PIN, LOW);
}

void connect_to_wifi() {
  WiFi.mode(WIFI_STA);
  Serial.println("---------- ATEMPTING TO CONNECT TO WIFI ----------");
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to `");
  Serial.print(SSID);
  Serial.print("`...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (i >= CONNECTING_THRESHOLD) {
      Serial.println("Failed to connect to WIFI. Please check wifi name and password");
    } else {
      Serial.print(".");
      i++;
    }
    delay(1000);
  }

  Serial.print("\n");
  Serial.println("Connection established!");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String setup_body(float *data, uint32_t size) {
  String body = "";
  if (size == 1) {
    String field = "&field1=";
    body = body + field;
    body = body + data[0];
  } else {
    String field = "&field1=";
    body = body + field;
    body = body + data[0];
    for (int i = 2; i <= size; ++i) {
      String field = "&field";
      body = body + field;
      body = body + i;
      body = body + "=";
      body = body + data[i - 1];
    }
  }

  return body;
}

void upload_data(float *data, uint32_t size) {
  String body = setup_body(data, size);
  if (client.connect(THINGSPEAK_URL, THINGSPEAK_PORT)) {
    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: ");
    client.print(THINGSPEAK_WRITE_KEY);
    client.print("\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\n\n");
    client.print(body);
    client.print("\n\n");
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(YFS201_DATA_IN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  delay(10);
  connect_to_wifi();
  ThingSpeak.begin(client);

  attachInterrupt(digitalPinToInterrupt(YFS201_DATA_IN), increase, RISING);
  water_timer = millis();
  power_timer = millis();
}

void loop() {
  // Wait a few seconds between measurements.
  delay(1000);
  float flow_rate = calculate_flow_rate();
  float power_usage = calculate_power_usage();
  float data[] = { power_usage, total_power_usage, flow_rate, total_volume };

  check_if_power_usage_overflow();
  check_if_water_usage_overflow();

  upload_data(data, (sizeof(data) / sizeof(data[0])));

  for (int i = 0; i < (sizeof(data) / sizeof(data[0])); ++i) {
    Serial.println(data[i]);
  }
  Serial.println("-----------------------------------");
}
