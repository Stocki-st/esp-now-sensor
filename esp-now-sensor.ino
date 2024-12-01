/***************************************************************
  ESP8266 wireless 1-wire sensor

  This projects implement a ESP8266 Temperature sensor module,
  which sends the temperature values in a pre-defined intervall
  to the esp-now receiver. While not sending, the device is in
  deep-sleep mode

  Author: Stefan Stockinger
  Mail:   mail.stefanstockinger@gmail.com
  Date:   03.12.2023
***************************************************************/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define UNIQUE_DEVICE_ID 0x01
#define SEND_INTERVALL_SECONDS 1800
//Firmware version
#define VER_MAJOR 0x01
#define VER_MINOR 0x01

#define ONE_WIRE_PIN 4 // on ESP8266

//#define DEBUG_PRINT

// overview of sensor adresses
//sensor 1: 0x28, 0x47, 0xCB, 0x57, 0x04, 0x71, 0x3C, 0x13
//sensor 2: 0x28, 0xC0, 0xB7, 0x57, 0x04, 0x11, 0x3C, 0x6A
//sensor 3: 0x28, 0x14, 0x44, 0x57, 0x04, 0xD8, 0x3C, 0x9C
//sensor 4: 0x28, 0xB2, 0xC5, 0x57, 0x04, 0x92, 0x3C, 0x08
//sensor 5: 0x28, 0xEF, 0xC5, 0x57, 0x04, 0x08, 0x3C, 0x48
const DeviceAddress sensor1 = { 0x28, 0xEF, 0xC5, 0x57, 0x04, 0x08, 0x3C, 0x48};
const DeviceAddress sensor2 = { 0x28, 0xEF, 0xC5, 0x57, 0x04, 0x08, 0x3C, 0x48};

// RECEIVER Board MAC Address
//uint8_t receiverAdress[] = {0x08, 0x3A, 0xF2, 0x71, 0x32, 0x0C}; //with display
uint8_t receiverAdress[] = {0x08, 0xAA, 0xB5, 0x8B, 0x01, 0xC8}; //AZ board

// Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire(ONE_WIRE_PIN);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

enum sensor_states {
  SUCCESS = 0,
  FAILED,
  NOT_USED
};

typedef struct struct_sensor {
  uint8_t state;
  float value;
} struct_sensor;

typedef struct struct_version {
  uint8_t major;
  uint8_t minor;
} struct_version;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  uint8_t id; // must be unique for each sender board
  struct_sensor sensor_1;
  struct_sensor sensor_2;
  struct_version ver;
  uint32_t intervall;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback when data is sent
void OnDataSent(uint8_t *mac_addr, uint8_t status) {
  Serial.printf("\r\nPacket Send Status: %d \r\n", status);
}

void setup() {
  myData.id = UNIQUE_DEVICE_ID;
  myData.ver.major = VER_MAJOR;
  myData.ver.minor = VER_MINOR;
  myData.intervall = SEND_INTERVALL_SECONDS;

  // Init Serial Monitor
  Serial.begin(115200);
  while (!Serial);
#ifdef DEBUG_PRINT
  Serial.println("Waking up...");
#endif
  // init 1-wire
  sensors.begin();

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_add_peer(receiverAdress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

void send_sensor_values() {
  sensors.requestTemperatures();
  float temperature = sensors.getTempC(sensor1);

  if (temperature <= -120) { //1-wire error =-127°C
    myData.sensor_1.state = FAILED;
  } else {
    myData.sensor_1.state = SUCCESS;
    myData.sensor_1.value = temperature;
  }

  temperature = sensors.getTempC(sensor2);

  if (temperature <= -120) { //1-wire error =-127°C
    myData.sensor_2.state = FAILED;
  } else {
    myData.sensor_2.state = SUCCESS;
    myData.sensor_2.value = temperature;
  }

#ifdef DEBUG_PRINT
  Serial.println("Value 1: ");
  Serial.println(myData.sensor_1.value);
  Serial.println("Value 2: ");
  Serial.println(myData.sensor_2.value);
#endif

  // Send message via ESP-NOW
  const uint8_t max_retries = 3;
  for ( int retry = 0; retry < max_retries; ++retry) {
    uint8_t res = esp_now_send(receiverAdress, (uint8_t *) &myData, sizeof(myData));

    if (res == 0) {
#ifdef DEBUG_PRINT
      Serial.println("Sent with success");
#endif
      break;
    } else {
#ifdef DEBUG_PRINT
      Serial.println("Error sending the data");
#endif
    }
  }
}

void start_deep_sleep() {
  Serial.println("Going to deep sleep...");
  ESP.deepSleep(SEND_INTERVALL_SECONDS * 1000000);
  yield();
}

void loop() {
  send_sensor_values();
  start_deep_sleep();
