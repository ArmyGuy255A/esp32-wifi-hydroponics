#include <Wire.h>
#include <WiFi.h>
#include "HT_SSD1306Wire.h"
#include "HT_DisplayUI.h"
#include <WebServer.h>

// OLED display
#ifdef Wireless_Stick_V3
SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_64_32, RST_OLED); // addr , freq , i2c group , resolution , rst
#else
SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst
#endif

DisplayUi ui( &display );
WebServer server(80);

// PINS
const int button1Pin = 38; // Toggle mode button
const int button2Pin = 37; // Increase value button
const int button3Pin = 36; // Decrease value button

// Debounce settings
unsigned long lastDebounceTime = 0;
const int debounceDelay = 250;

// Mode and value variables
enum Mode { Runtime, Triggertime };
Mode currentMode = Runtime;
int runtimeValue = 60000;
int triggerTimeValue = 900000;
bool vextOn = false;

unsigned long lastActivationTime = 0;
unsigned long lastDeactivationTime = 0;
bool manualActivation = false;


// WiFi credentials
const char* ssid = "Appeid.com 2.4GHz";
const char* password = "MrArmyGuy!!11";


void connectToWifi() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setHandler() {
  String type = server.uri().substring(1); // Extract "setTrigger" or "setRuntime" from the URL

  if (server.hasArg("value")) {
    int *valuePtr = type == "setRuntime" ? &runtimeValue : &triggerTimeValue;
    String valueType = type == "setRuntime" ? "Runtime " : "Trigger time ";
    (*valuePtr) = server.arg("value").toInt();
    server.send(200, "text/plain", valueType + "updated to " + millisToReadableTime(*valuePtr));
  } else {
    server.send(400, "text/plain", "Bad Request: Missing value argument");
  }
}

void setupWebServer() {
  server.on("/waterPump", HTTP_POST, activateWaterPumpManually);
  server.on("/setTrigger", HTTP_POST, setHandler);
  server.on("/setRuntime", HTTP_POST, setHandler);
  server.begin();
}

void updateProgressBar(int progress, String currentStepText) {
  display.clear();
  display.drawString(0, 0, currentStepText);
  display.drawProgressBar(0, 20, 120, 10, progress);
  display.display();
  delay(250);
}

void setup() {
  Serial.begin(115200);

  // Turn off Voltage External (Relay)
  VextOFF();

  // Initialize buttons
  pinMode(button1Pin, INPUT);
  pinMode(button2Pin, INPUT);
  pinMode(button3Pin, INPUT);

  display.init();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  updateProgressBar(25, "Initializing...");

  updateProgressBar(50, "Connecting to WiFi...");
  connectToWifi();

  updateProgressBar(75, "NTP Sync...");
  configTime(-8, 0, "pool.ntp.org"); // Set timezone offset and daylight offset to 0, configure NTP server

  updateProgressBar(100, "REST Server...");
  setupWebServer();
}

String millisToReadableTime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  seconds = seconds % 60;

  String readableTime = String(minutes) + " min " + String(seconds) + " sec";
  return readableTime;
}

void displayInfo() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  display.clear();
  display.setFont(ArialMT_Plain_10);

  String triggerString = "Trigger: ";
  String runString = "Run: ";

  switch (currentMode) {
    case Runtime:
      runString = String("> Run: ");
      break;
    case Triggertime:
      triggerString = String("> Trigger: ");
      break;
    default:
      break;
  }

  display.drawString(0, 0, "WiFi: " + String(ssid));
  display.drawString(0, 10, "IP: " + WiFi.localIP().toString());
  display.drawString(0, 20, triggerString + millisToReadableTime(triggerTimeValue));
  display.drawString(0, 30, runString + millisToReadableTime(runtimeValue));
  display.drawString(0, 40, "Time: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));

  // Pump Off or Next Run
  String statusString = vextOn ? "Pump Off: " : "Next Run: ";
  unsigned long remainingTime = vextOn ? runtimeValue -  (millis() - lastActivationTime) : triggerTimeValue -  (millis() - lastDeactivationTime);
  display.drawString(0, 50, statusString + millisToReadableTime(remainingTime));

  display.display();
}

void activateWaterPump() {
  Serial.println("Water Pump Activated");

  if (vextOn == true) {
    VextOFF();
  } else {
    VextON();
  }

  //TODO: I need to clear the screen and show a progress bar.

  // The progress bar should show the total time remaining.

  // The default runtime for the pump is 60 seconds
}

void activateWaterPumpManually() {
  const char* activationText = "Water Pump Activated Manually";

  Serial.println(activationText);
  manualActivation = true;
  server.send(200, "text/plain", activationText);
  activateWaterPump();
}

void readButtons() {
  // Read button states
  int button1State = digitalRead(button1Pin);
  int button2State = digitalRead(button2Pin);
  int button3State = digitalRead(button3Pin);

  // Check if any button is pressed and debounce
  if ((button1State == LOW || button2State == LOW || button3State == LOW) && (millis() - lastDebounceTime) > debounceDelay) {
    // Update debounce time
    lastDebounceTime = millis();

    // Toggle mode
    if (button1State == LOW) {
      currentMode = (currentMode == Runtime) ? Triggertime : Runtime;
      Serial.println("Mode Pushed");
    }

    // Grab a pointer for the runtime or trigger time and just increment it.
    int *value = (currentMode == Runtime) ? &runtimeValue : &triggerTimeValue;

    // Increase value
    if (button2State == LOW && *value < 3600000 ) {
      Serial.println("Up Pushed");
      (*value) += 1000;
    }

    // Decrease value
    if (button3State == LOW && *value != 0) {
      Serial.println("Down Pushed");
      (*value) -= 1000;
    }
  }
}

void VextON() {
  Serial.println("Vext ON");
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  vextOn = true;
  lastActivationTime = millis();
  // lastDeactivationTime = 0; // Reset the deactivation timer
}

void VextOFF() {
  Serial.println("Vext OFF");
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
  vextOn = false;
  lastDeactivationTime = millis();
}


void loop() {
  // Read button states
  readButtons();

  // Your main code
  displayInfo();

  // Need this to handle client requests on the REST server
  server.handleClient();

  // Check if it's time to activate the water pump
  if (!vextOn && (millis() - lastDeactivationTime) >= (triggerTimeValue)) {
  // if (!manualActivation && (millis() - lastActivationTime) >= (triggerTimeValue * 60 * 1000)) {
    activateWaterPump();    
  }

  // Check if it's time to deactivate the water pump
  // if (vextOn && (millis() - lastDeactivationTime) >= (runtimeValue * 60 * 1000)) {
  if (vextOn && (millis() - lastActivationTime) >= (runtimeValue)) {
    activateWaterPump();
    manualActivation = false; // Reset the manual activation flag
  }
}