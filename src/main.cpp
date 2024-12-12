#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Brain.h>

#include <HttpClient.h>
#include <WiFi.h>
#include <inttypes.h>
#include <stdio.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <Wire.h>
#include <Adafruit_AHTX0.h>

//BLE Service and Characteristic UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

//Set up brain parser pass to hardware serial object you want to listen on.
Brain brain(Serial);

//BLE Characteristic
BLECharacteristic *pCharacteristic;

//LED pins
//red
#define LED_PIN1 27

//yellow
#define LED_PIN2 26

//blue
#define LED_PIN3 33

//green
#define LED_PIN4 32

#define BUTTON_PIN 15
bool buttonPressed = false;


//AWS/Server details
const char kServerIP[] = "35.85.147.206";
const int kServerPort = 5000;

//Wi-Fi credentials
char ssid[50];
char pass[50];

//Current mode: "attention" or "meditation"
String mode = "attention";

//Callback class to handle write events from phone
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("New value from phone: ");
      for (int i = 0; i < (int)value.length(); i++)
        Serial.print(value[i]);
      Serial.println();
      Serial.println("*********");

      //Convert std::string to String
      String command = String(value.c_str());

      //Check if command is meditation or attention
      if (command.equalsIgnoreCase("meditation")) {
        mode = "meditation";
        Serial.println("Mode switched to: meditation");
      } else if (command.equalsIgnoreCase("attention")) {
        mode = "attention";
        Serial.println("Mode switched to: attention");
      } else {
        Serial.println("Unknown command. Mode unchanged.");
      }
    }
  }
};

void nvs_access() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  Serial.printf("\nOpening Non-Volatile Storage (NVS) handle... ");
  nvs_handle_t my_handle;
  err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK)
  {
    Serial.printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  else
  {
    Serial.printf("Done\nRetrieving SSID/PASSWD\n");
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);
    err = nvs_get_str(my_handle, "ssid", ssid, &ssid_len);
    err |= nvs_get_str(my_handle, "pass", pass, &pass_len);
    if (err == ESP_OK)
    {
      Serial.printf("SSID and Password retrieved successfully.\n");
    }
    else
    {
      Serial.printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
  }
  nvs_close(my_handle);
}

void sendMotivationalMessage(int attention) {
  String message;

  if (attention >= 0 && attention < 25) {
      message = "Stay determined! You can do it!";
  } else if (attention >= 25 && attention < 50) {
      message = "You're making steady progress!";
  } else if (attention >= 50 && attention < 75) {
      message = "Excellent effort! Keep it up!";
  } else {
      message = "Outstanding performance! You're on top!";
  }

  //Send the message via BLE
  Serial.println("Sending BLE Notification: " + message);
  pCharacteristic->setValue(message.c_str());
  pCharacteristic->notify();
}

void sendAttentionToServer(int attention) {
  //Create an HTTP client
  WiFiClient client;
  HttpClient http(client);

//Construct the HTTP request with attention as a parameter
String paramName = (mode == "attention") ? "attention" : "meditation";
String url = String("/update?") + paramName + "=" + attention;


  Serial.println("Sending HTTP request to server: " + url);

  int err = http.get(kServerIP, kServerPort, url.c_str(), NULL);

  //Handle the HTTP response
  if (err == 0) {
    Serial.println("HTTP request sent successfully.");
    err = http.responseStatusCode();
    if (err > 0) {
      Serial.printf("Response code: %d\n", err);
      http.skipResponseHeaders();
      int bodyLen = http.contentLength();
      while (bodyLen > 0) {
        char c = http.read();
        bodyLen--;
      }
      Serial.println();
    } else {
      Serial.printf("Failed to get response: %d\n", err);
    }
  } else {
    Serial.printf("HTTP request failed: %d\n", err);
  }

  http.stop();
}

void setup() {
  //Serial and hardware setup
  Serial.begin(9600, SERIAL_8N1, 25);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(LED_PIN3, OUTPUT);
  pinMode(LED_PIN4, OUTPUT);

   pinMode(BUTTON_PIN, INPUT_PULLUP);

  //Access Wi-Fi credentials and connect
  nvs_access();
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected!");
  Serial.println("IP address: " + WiFi.localIP().toString());

  //Initialize BLE
  BLEDevice::init("BrainAttentionMonitor");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  //Create a BLE characteristic (now with write property)
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | 
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_WRITE
  );

  pCharacteristic->setCallbacks(new MyCallbacks());

  //Initial BLE characteristic value
  pCharacteristic->setValue("Focus on the task!");
  pService->start();

  //Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE Brain Attention Monitor is ready.");
  Serial.println("Current mode: " + mode);
}

void loop() {
  //Check for brain sensor updates
  if (brain.update()) {
    int value;

    //Depending on the mode, read attention or meditation
    if (mode == "attention") {
      value = brain.readAttention();
    } else {
      value = brain.readMeditation();
    }

    //Log the value
    Serial.print(mode + " = ");
    Serial.println(value);

    //Update BLE with motivational message (assuming the same for both modes)
    sendMotivationalMessage(value);

    //Send value to server (treat value as 'attention' parameter for now)
    sendAttentionToServer(value);

    //Control LED based on score
    if (value >= 0 && value < 25) {
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, LOW);
      digitalWrite(LED_PIN3, LOW);
      digitalWrite(LED_PIN4, LOW);
    } else if (value >= 25 && value < 50) {
      digitalWrite(LED_PIN2, HIGH);
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN3, LOW);
      digitalWrite(LED_PIN4, LOW);
    } else if (value >= 50 && value < 75) {
      digitalWrite(LED_PIN3, HIGH);
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      digitalWrite(LED_PIN4, LOW);
    } else {
      digitalWrite(LED_PIN4, HIGH);
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      digitalWrite(LED_PIN3, LOW);
    }
  }

  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == HIGH && !buttonPressed) {
    buttonPressed = true; //Mark as pressed
    delay(50); //Debounce delay
    Serial.println("Button pressed.");

    //Toggle the mode
    if (mode == "attention") {
      mode = "meditation";
       Serial.println("*********");
      Serial.println("New value from button: meditation");
      Serial.println("*********");
    } else {
      mode = "attention";
      Serial.println("*********");
      Serial.println("New value from button: attention");
      Serial.println("*********");
    }
  }

  if (buttonState == LOW) {
    buttonPressed = false; //Reset only when button is released
  }
  //delay(10);
}
