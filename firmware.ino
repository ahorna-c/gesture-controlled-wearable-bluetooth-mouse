#include <Wire.h> // For I2C communication with MPU6050
#include <Adafruit_Sensor.h> // Base class for Adafruit sensors
#include <Adafruit_MPU6050.h> // Specific library for MPU6050

#include <BLEDevice.h> // For BLE functionality
#include <BLEUtils.h>  // BLE utility functions
#include <BLEServer.h> // For creating a BLE server
#include <BLE2902.h>   // For BLE descriptor for notifications (enhances compatibility)

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID "55925ea2-e795-4816-b8d5-e0dcf7c09b15"
#define CHAR_UUID    "2823aff6-4924-4f5a-8fe9-fbebe5a97f93"

// BLE Characteristic object for sending notifications
BLECharacteristic* pCharacteristic;
bool deviceConnected = false; // Flag to track connection status

// MPU6050 Configuration
Adafruit_MPU6050 mpu; // Create an instance of the MPU6050 sensor object
const int MPU_ADDR = 0x68; // I2C address of the MPU-6050 (usually 0x68)
const int I2C_SDA_PIN = 21; // ESP32 default SDA pin
const int I2C_SCL_PIN = 22; // ESP32 default SCL pin

const int button1Pin = 19; // Button 1 - typically used for left-click or mode toggle
const int button2Pin = 4;
const int button3Pin = 18;
const int button4Pin = 5;
const int button5Pin = 23;

// Variables to store the current and previous button states for debouncing
// Assuming INPUT_PULLUP, so HIGH is unpressed, LOW is pressed
int button1State; int lastButton1State = HIGH;
int button2State; int lastButton2State = HIGH;
int button3State; int lastButton3State = HIGH;
int button4State; int lastButton4State = HIGH;
int button5State; int lastButton5State = HIGH;

const long debounceDelay = 50; // Debounce time in milliseconds
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
unsigned long lastDebounceTime5 = 0;

// This flag determines if tilt movements control the cursor or scrolling.
// False = Cursor mode, True = Scrolling mode
bool isScrollingMode = false;

// Data for BLE Transmission
float currentRollAngle = 0.0;
float currentPitchAngle = 0.0;
int currentButtonMask = 0;

// Interval for sending BLE notifications (e.g., 50ms for smoother control)
const unsigned long BLE_UPDATE_INTERVAL_MS = 10;
unsigned long lastBLEUpdateTime = 0;

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected!");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected!");
        // Restart advertising to become discoverable again
        BLEDevice::startAdvertising(); // This restarts advertising for any BLEDevice instance
        Serial.println("Advertising restarted.");
    }
};

void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging
  while (!Serial) { delay(10); } // Wait for serial port to connect (for some boards)
  Serial.println("ESP32 BLE Gesture Mouse Started!");

  // Initialize Button Pins
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(button4Pin, INPUT_PULLUP);
  pinMode(button5Pin, INPUT_PULLUP);
  Serial.println("Button pins configured.");

  // Initialize I2C and MPU6050
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Initialize I2C communication
  if (!mpu.begin(MPU_ADDR)) { // Attempt to initialize MPU6050
    Serial.println("Failed to find MPU6050 chip. Check wiring, power, and I2C address (0x68).");
    while (1) { delay(10); } // Halt program execution if sensor is not found
  }
  Serial.println("MPU6050 Found and Initialized.");
 
  // Configure MPU6050 sensor ranges for optimal performance
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);      // Set accelerometer range to +/- 8G
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);           // Set gyroscope range to +/- 250 degrees/second
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);        // Set Digital Low Pass Filter (DLPF) bandwidth
  Serial.println("MPU-6050 Configured.");

  // Initialize BLE Server
  BLEDevice::init("NanoESP32-BLE"); // Initialize BLE with the device name
  BLEServer* pServer = BLEDevice::createServer(); // Create a BLE server
  pServer->setCallbacks(new MyServerCallbacks()); // Set the custom callback for server events

  BLEService* pService = pServer->createService(SERVICE_UUID); // Create a BLE service

  // Create the BLE Characteristic with Notify property
  pCharacteristic = pService->createCharacteristic(
    CHAR_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY // Enable notifications
  );

  pCharacteristic->addDescriptor(new BLE2902()); // Add a descriptor for better compatibility
  // No initial value needed for pCharacteristic as it will be updated dynamically

  pService->start(); // Start the BLE service

  // Start BLE Advertising to allow devices to discover and connect
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); // Advertise your service UUID
  pAdvertising->setScanResponse(true); // Ensure scan responses are enabled for better discoverability

  // Set preferred connection parameters
  // Units are 1.25ms. 0x06 = 7.5ms, 0x12 = 22.5ms.
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
 
  BLEDevice::startAdvertising(); // Start advertising

  Serial.println("BLE server started and advertising.");
  Serial.println("Press Button 1 to toggle between CURSOR and SCROLLING modes.");
}

void loop() {
  unsigned long currentTime = millis(); // Get the current time for timing operations

  // Button Handling Logic
  // Reads button states, applies debouncing, and updates currentButtonMask.

  // Button 1
  int reading1 = digitalRead(button1Pin);
  if (reading1 != lastButton1State) { lastDebounceTime1 = currentTime; }
  if ((currentTime - lastDebounceTime1) > debounceDelay) {
    if (reading1 != button1State) {
      button1State = reading1;
      if (button1State == LOW) { // Button 1 is PRESSED
        Serial.println("BUTTON 1 PRESSED!");
        currentButtonMask |= (1 << 0); // Set bit 0 in the mask
        // Toggle the scrolling mode when Button 1 is pressed
        isScrollingMode = !isScrollingMode;
        if (isScrollingMode) {
          Serial.println("MODE: NOW SCROLLING");
        } else {
          Serial.println("MODE: CURSOR");
        }
      } else { // Button 1 is RELEASED
        Serial.println("BUTTON 1 RELEASED!");
        currentButtonMask &= ~(1 << 0); // Clear bit 0 in the mask
      }
    }
  }
  lastButton1State = reading1;

  // Button 2
  int reading2 = digitalRead(button2Pin);
  if (reading2 != lastButton2State) { lastDebounceTime2 = currentTime; }
  if ((currentTime - lastDebounceTime2) > debounceDelay) {
    if (reading2 != button2State) {
      button2State = reading2;
      if (button2State == LOW) { Serial.println("BUTTON 2 PRESSED!"); currentButtonMask |= (1 << 1); } else { Serial.println("BUTTON 2 RELEASED!"); currentButtonMask &= ~(1 << 1); }
    }
  }
  lastButton2State = reading2;

  // Button 3
  int reading3 = digitalRead(button3Pin);
  if (reading3 != lastButton3State) { lastDebounceTime3 = currentTime; }
  if ((currentTime - lastDebounceTime3) > debounceDelay) {
    if (reading3 != button3State) {
      button3State = reading3;
      if (button3State == LOW) { Serial.println("BUTTON 3 PRESSED!"); currentButtonMask |= (1 << 2); } else { Serial.println("BUTTON 3 RELEASED!"); currentButtonMask &= ~(1 << 2); }
    }
  }
  lastButton3State = reading3;

  // Button 4
  int reading4 = digitalRead(button4Pin);
  if (reading4 != lastButton4State) { lastDebounceTime4 = currentTime; }
  if ((currentTime - lastDebounceTime4) > debounceDelay) {
    if (reading4 != button4State) {
      button4State = reading4;
      if (button4State == LOW) { Serial.println("BUTTON 4 PRESSED!"); currentButtonMask |= (1 << 3); } else { Serial.println("BUTTON 4 RELEASED!"); currentButtonMask &= ~(1 << 3); }
    }
  }
  lastButton4State = reading4;

  // Button 5
  int reading5 = digitalRead(button5Pin);
  if (reading5 != lastButton5State) { lastDebounceTime5 = currentTime; }
  if ((currentTime - lastDebounceTime5) > debounceDelay) {
    if (reading5 != button5State) {
      button5State = reading5;
      if (button5State == LOW) { Serial.println("BUTTON 5 PRESSED!"); currentButtonMask |= (1 << 4); } else { Serial.println("BUTTON 5 RELEASED!"); currentButtonMask &= ~(1 << 4); }
    }
  }
  lastButton5State = reading5;


  // Accelerometer Handling Logic
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp); // Get accelerometer data

  // Calculate Pitch and Roll from Accelerometer data
  currentRollAngle = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / PI;
  currentPitchAngle = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;

  // Data Packet Formulation and BLE Notification
  // Only send notifications if a device is connected AND enough time has passed
  if (deviceConnected && (currentTime - lastBLEUpdateTime >= BLE_UPDATE_INTERVAL_MS)) {
    // Format the data into a comma-separated string: "Pitch,Roll,ButtonMask,IsScrollingMode"
    // Pitch and Roll are formatted to 2 decimal places for consistent string length.
    String dataToSend = String(currentPitchAngle, 2) + "," +
                        String(currentRollAngle, 2) + "," +
                        String(currentButtonMask) + "," +
                        String(isScrollingMode ? 1 : 0); // Send 1 if true, 0 if false

    // Set the BLE characteristic's value and notify connected clients
    pCharacteristic->setValue(dataToSend.c_str());
    pCharacteristic->notify(); // Send the notification

    Serial.print("BLE Sent: ");
    Serial.println(dataToSend); // For debugging on ESP32 Serial Monitor

    lastBLEUpdateTime = currentTime; // Update the last send time
  }

  // A small delay to prevent overwhelming the ESP32's processor
  // This delay allows other background BLE tasks to run smoothly.
  delay(1);
}
