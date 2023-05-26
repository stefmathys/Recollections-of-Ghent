#include <CapacitiveSensor.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

// Initialise variables for ledstrips
#define PIN_STRIP1 6
#define PIN_STRIP2 3
#define NUM_PIXELS1 50
#define NUM_PIXELS2 60
Adafruit_NeoPixel Strip1 = Adafruit_NeoPixel(NUM_PIXELS1, PIN_STRIP1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel Strip2 = Adafruit_NeoPixel(NUM_PIXELS2, PIN_STRIP2, NEO_GRB + NEO_KHZ800);

// Initialise variables for capacitive sensors
CapacitiveSensor sensor1 = CapacitiveSensor(4, 2);
CapacitiveSensor sensor2 = CapacitiveSensor(4, 7);

// Define variables for peak detection
const int numReadings = 10;
int readings1[numReadings];
int readings2[numReadings];
int index1 = 0;
int index2 = 0;
int total1 = 0;
int total2 = 0;
int average1 = 0;
int average2 = 0;
int threshold1 = 0;
int threshold2 = 0;
String readCommand;
bool mode = false;      // Boolean to check the current mode of the application. false = discover, true = store.
bool power = false;     // Boolean to check if the application has been turned on with the hand-shaped capacitive sensor
bool tutorial = false;  // Boolean to check if the tutorial has been completed
bool buttonsOn = true;  // Set to false to disable the buttons if necessary
long sensorValue1;
long sensorValue2;
unsigned long tutorialTime;
unsigned long offTime;

// Define variables to your liking
int thresholdPercent = 85;  // Percentage of the peak detection that counts as threshold value
int speed = 100;            // Speed of the ledstrip pulse
int brightness = 70;        // Brightness of the ledstrips

void setup() {
  // Disable the autocalibration of the capacitive sensors
  sensor1.set_CS_AutocaL_Millis(0xFFFFFFFF);
  sensor2.set_CS_AutocaL_Millis(0xFFFFFFFF);

  // Enable the ledstrips and set the brightness
  Strip1.begin();
  Strip2.begin();
  Strip1.clear();
  Strip2.clear();
  Strip1.setBrightness(brightness);
  Strip2.setBrightness(brightness);
  Strip1.show();
  Strip2.show();

  Serial.begin(9600);

  // Calibrate the startvalues for the capacitive sensors
  capSensCalibration();

  // Wait one second before measuring starts
  delay(1000);
}

void loop() {
  // Clear but don't show the values of the ledstrips to prevent unwanted results
  Strip1.clear();
  Strip2.clear();

  // Read the sensor values and assign them to variables
  sensorValue1 = sensor1.capacitiveSensor(30);
  sensorValue2 = sensor2.capacitiveSensor(30);

  // Read the serial or dashboard input
  readCommand = Serial.readString();
  readCommand.trim();  // remove any \r \n whitespace at the end of the String

  // If needed, send "Calibrate" as a Serial Message to force a new calibration
  if (readCommand == "Calibrate") {
    capSensCalibration();
    readCommand = " ";
  }

  // If needed, send "Buttons" as a Serial Message to turn off/on the buttons
  if (readCommand == "Buttons") {
    if (buttonsOn == true) {
      buttonsOn = false;
      Serial.println("Buttons turned off");
    } else {
      buttonsOn = true;
      Serial.println("Buttons turned on");
    }
    readCommand = " ";
  }

  // Disable the buttons as long as the tutorial is playing
  if (power == 1) {
    tutorialTime = millis() - offTime;
  } else {
    offTime = millis();
  }
  if (tutorialTime > 150000) {
    tutorial = true;
  }

  // If the power has been turned on, check if the threshold for the first sensor is reached
  // If needed, force a modeSwitch by typing Switch in the monitor
  if (power == true) {
    if ((sensorValue1 > threshold1 && buttonsOn == true && tutorial == true) || readCommand == "Switch") {
      // Trigger for Protopie Connect
      Serial.println("modeSwitch");
      readCommand = " ";
      // Assign colour to the first ledstrip according to the current mode
      if (mode == false) {
        mode = true;
        Strip1.clear();
        // Assign yellow to the non-active (pressable) button
        for (int pixel = 0; pixel < 25; pixel++) {                 // Pink = 245, 43, 53
          Strip1.setPixelColor(pixel, Strip1.Color(30, 40, 130));  // Yellow = 198, 212, 12
        }                                                          // Blue = 30, 40, 130
      } else if (mode == true) {
        mode = false;
        Strip1.clear();
        // Assign pink to the non-active (pressable) button
        for (int pixel = 26; pixel < 50; pixel++) {
          Strip1.setPixelColor(pixel, Strip1.Color(30, 40, 130));
        }
      }
      Strip1.show();

      // Recalibration to prevent bugs in the measurements as much as possible
      capSensCalibration();
    }
  }

  // As long as the power is not turned on, the ledstrip underneath the hand will pulse
  if (power == false) {
    ledStripPulse();
  }

  // Short delay between measurements
  delay(10);
}

// Capacitive readings can be uncontrollable due to a wide variety of factors such as nearby electric interference, grounding, room factors, ...
// To limit the influence of this often random behaviour, the readings use relative values measured from a normal reading during the calibration
// When the values reach a relative peak compared to this normal reading, it is counted as an activation of the sensor
// In order to maintain a steady use of the interactive table, calibration is performed after every peak reading to reset the measurements
void capSensCalibration() {
  Serial.println("Calibrating sensors...");

  // Set all calibration values back to 0
  index1 = 0;
  index2 = 0;
  total1 = 0;
  total2 = 0;
  threshold1 = 0;
  threshold2 = 0;

  delay(2000);

  // Read the sensors a couple of times and save the results in matrices
  for (int i = 0; i < numReadings; i++) {
    readings1[i] = sensor1.capacitiveSensor(30);
    readings2[i] = sensor2.capacitiveSensor(30);
    delay(10);
  }

  // Calculate the mean values of the readings
  for (int i = 0; i < numReadings; i++) {
    total1 += readings1[i];
    total2 += readings2[i];
  }
  average1 = abs(total1 / numReadings);
  average2 = abs(total2 / numReadings);

  // Multiply the average readings with a set value to reach a threshold value
  threshold1 = max(average1 * thresholdPercent / 10, average1 + max(.2 * average1, 250));  // Set a minimumvalue of either the average + 150 or 120% of the average
  while (threshold1 - average1 > 1000) {                                                   // Limit the threshold if it is too high
    threshold1 -= 500;
  }
  threshold2 = max(average2 * thresholdPercent / 10, average2 + max(.2 * average2, 250));
  while (threshold2 - average2 > 1000) {
    threshold2 -= 500;
  }

  // Debug information, shortened to fit on the Protopie Connect command message length
  Serial.print("S1 - av: ");
  Serial.print(average1);
  Serial.print(", tv: ");
  Serial.println(threshold1);
  Serial.print("S2 - av: ");
  Serial.print(average2);
  Serial.print(", tv: ");
  Serial.println(threshold2);
  Serial.println("Calibration complete");
}

// Measure the second capacitive sensor, underneath the hand, to use in the pulse function
void capSens2Meting() {
  // Check if the threshold for the first sensor is reached
  if (sensorValue2 > threshold2) {
    // Trigger for Protopie Connect
    Serial.println("turnOnOff");
    power = true;

    // Assign colour to the second ledstrip
    Strip2.clear();
    // Assign blue to the non-active (pressable) button
    for (int pixel = 26; pixel < 50; pixel++) {
      Strip1.setPixelColor(pixel, Strip1.Color(30, 40, 130));
    }
    Strip1.show();
    Strip2.show();

    // Recalibration to prevent bugs in the measurements as much as possible
    capSensCalibration();
  }
}

// As long as the power is not turned on, the ledstrip underneath the hand will pulse
void ledStripPulse() {
  Strip1.clear();
  Strip2.clear();

  //New
  if (power == false) {
    Strip2.setPixelColor(41, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(41, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(21, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(22, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(40, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(42, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(59, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(21, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(22, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(40, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(42, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(59, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(4, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(20, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(23, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(39, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(43, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(58, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(4, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(20, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(23, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(39, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(43, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(58, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(3, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(5, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(19, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(24, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(38, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(44, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(57, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(3, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(5, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(19, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(24, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(38, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(44, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(57, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(2, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(6, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(18, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(25, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(37, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(45, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(56, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(2, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(6, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(18, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(25, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(37, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(45, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(56, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(1, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(7, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(17, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(26, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(36, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(46, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(55, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(1, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(7, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(17, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(26, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(36, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(46, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(55, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(0, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(8, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(16, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(27, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(35, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(47, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(54, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(0, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(8, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(16, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(27, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(35, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(47, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(54, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(9, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(15, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(28, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(34, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(48, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(53, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(9, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(15, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(28, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(34, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(48, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(53, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(10, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(14, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(29, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(33, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(49, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(52, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(10, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(14, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(29, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(33, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(49, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(52, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(11, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(13, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(30, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(32, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(50, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(51, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //Repeat
    Strip2.setPixelColor(11, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(13, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(30, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(32, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(50, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(51, Strip2.Color(30, 40, 130));
    //New
    Strip2.setPixelColor(12, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(31, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    //New
    Strip2.setPixelColor(12, Strip2.Color(30, 40, 130));
    Strip2.setPixelColor(31, Strip2.Color(30, 40, 130));
    Strip2.show();
    delay(speed);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }

  if (power == false) {
    Strip2.clear();
    Strip2.show();
    delay(1000);
    capSens2Meting();  // Check if the threshold for the second sensor is reached
  }
}