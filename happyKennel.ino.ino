#include <WiFiNINA.h>
#include <Arduino_MKRIoTCarrier.h>
#include <ThingSpeak.h>
#include <Arduino_JSON.h>
#include "secrets.h"
#include "sensorsConfig.h"
#include "images.h"

// #include <ArduinoJson.h>
#include <BlynkSimpleWifi.h>
#include <SPI.h>

#define NUMPIXELS 5

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;
MKRIoTCarrier carrier;
char ssid[] = WIFI_NAME;        // your network SSID (name)
char pass[] = WIFI_PASSWORD;    // your network password (use for WPA, or use as key for WEP)
char appid[] = SECRET_APPID;
char weather_loc[] = SECRET_LOC;
char lat[] = SECRET_LAT;
char lon[] = SECRET_LON;

const char server[] = "api.openweathermap.org";
const long interval = 10000; // milliseconds
const int calibrate = -2; // Temperature calibration
String weather = "";
String town = "";
float temp_out = 0;
float windSpeed = 0;

int button_state = 0;
int backlight_state = HIGH;
unsigned long previous_t = 0;
unsigned long current_t = 0;

float temp_in = 0;
float humidity = 0;
int moisture = 0;
float gyroScope = 0;
float gyroMagnitude = 0;
int shakePercentage = 0;


int status = WL_IDLE_STATUS;
const char* mqttServer = "mqtt3.thingspeak.com";  // Replace with your MQTT Broker address
const int mqttPort = 1883;                  // typical MQTT port

BlynkTimer timer;
WiFiClient wifiClient;

void setup() {
  CARRIER_CASE = false;
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  setupWiFi();

  ///LED to mimic heater switching on/off
  pinMode(2, OUTPUT);   // LED

  PIRSensorSetup();

  carrier.begin();

  carrier.display.setRotation(0);
  uint16_t color = 0x04B3; // Arduino logo colour
  carrier.display.fillScreen(ST77XX_BLACK);  
  carrier.display.drawBitmap(44, 60, ArduinoLogo, 152, 72, color);
  carrier.display.drawBitmap(48, 145, ArduinoText, 144, 23, color);
  delay(1000);

  ThingSpeak.begin(wifiClient);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(1000L, writeToBlynkApp);

}

void loop() {

  buttonControl(temp_in);
  // call request data from OpenWeather app and parse for reading
  requestData();
  parseData();

  // update current readings from sensors
  updateSensors();

  // call all sensorConfig methods
  heaterController(temp_in);
  getMoistureText(moisture);
  pirSensorLoop();
  getShakeSensorValue(gyroScope);

  Serial.println("InSide Temp:" +String(temp_in));
  Serial.println("OutSide Temp:" +String(temp_out));
  Serial.println("Kennel Shake %:" +String(shakePercentage));
  Serial.println("PIR:" +String(pir));
  Serial.println("Moisture:" +String(moisture));
  Serial.println("Humidity:" +String(humidity));
  Serial.println("WindSpeed:" +String(windSpeed));
  
   // set the fields with the values for ThingSpeak
  ThingSpeak.setField(1, temp_in);
  ThingSpeak.setField(2, temp_out);
  ThingSpeak.setField(3, shakePercentage);
  ThingSpeak.setField(4, pir);
  ThingSpeak.setField(5, moisture);
  ThingSpeak.setField(6, humidity);
  ThingSpeak.setField(7, windSpeed);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
   if(x == 200){
    Serial.println("ThingSpeak Channel update successful.");
  }
  else{
    Serial.println("Problem updating THingSpeak channel. HTTP error code " + String(x));
  }
  delay(INTERVAL);  // Delay for interval
  
  // Initiate Blynk app
  Blynk.run();
  timer.run();
}

// this method updates arduino sensors
void updateSensors(){

  temp_in = carrier.Env.readTemperature();
  humidity = carrier.Env.readHumidity();
  moisture = analogRead(A6);
  pir = digitalRead(pirPin);
  gyroScope = carrier.IMUmodule.readGyroscope(gX, gY, gZ);
  gyroMagnitude = sqrt(gX * gX + gY * gY + gZ * gZ);
  shakePercentage = map(gyroMagnitude, 0, 100, 0, 100);

}

// this method displays arduino temperature when button 0 is pressed
void buttonControl(float temp_in) {
  carrier.Buttons.update(); // Check touch pads 
  if (carrier.Buttons.onTouchChange(TOUCH0)) {
    showButtonLed(0);
    // updateSensors();
    showValue("Indoor", temp_in + calibrate, "C");
    Serial.println("ButtonControl:" +String(temp_in + calibrate));
    changeState(0);      
  }

  current_t = millis();
  if (current_t - previous_t >= interval) {
    previous_t = current_t;
    //Turn off backlight to save energy
    if (backlight_state == HIGH) {
        backlight_state = LOW;
        digitalWrite(TFT_BACKLIGHT, backlight_state);
      }
    }
  delay(10); // Short delay
}


void changeState(const int state) {
  previous_t = millis();      
  button_state = state;
  delay(250); // Delay for stability 
}

// this method lights up RGB light at selected button
void showButtonLed(const int button) {
  carrier.Buzzer.sound(30);
  carrier.leds.setPixelColor(button, 0);
  carrier.leds.show();
  delay(50);  // Buzz length
  carrier.Buzzer.noSound();
}

// This method shows arduino temperature on the carrier display 
void showValue(const char* name, float temp, const char* unit) {
  previous_t = millis();  
  if (backlight_state == LOW) {
    backlight_state = HIGH;
    digitalWrite(TFT_BACKLIGHT, backlight_state);
  }
  carrier.display.fillScreen(ST77XX_BLUE);
  carrier.display.setCursor(60, 50);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(3);
  carrier.display.print("InSide Temp");
  carrier.display.setTextSize(7);
  // Centre display
  if (temp > 9) {
    carrier.display.setCursor(50, 110);
  }
  else {
    carrier.display.setCursor(40, 110);
  }
  carrier.display.print(String(temp, 0));
  carrier.display.print(unit);
}


// THis function reads the sensors from the arduino carrier and writes the data to the Blynk app virtual pins.
void writeToBlynkApp() {

  updateSensors();

  Blynk.virtualWrite(V0, shakePercentage);
  Blynk.virtualWrite(V1, temp_in);
  Blynk.virtualWrite(V2, temp_out);
  Blynk.virtualWrite(V3, moisture);
  Blynk.virtualWrite(V4, pir);

  Serial.println("BlynkShake:" +String(shakePercentage));
  Serial.println("BlynkInTemp:" +String(temp_in));
  Serial.println("BlynkOutTemp:" +String(temp_out));
  Serial.println("BlynkMoist:" +String(moisture));
  Serial.println("BlynkPIR:" +String(pir));
}


void setupWiFi() {
 // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(INTERVAL);
  }
  // you're connected now, so print out the data:
  Serial.println("You're connected to the network");
}


// Disconnect WiFi to save energy - if using arduino remotely with battery
// void disconnectWiFi() {
//   client.stop();
//   if (WiFi.status() == WL_CONNECTED) {
//     WiFi.disconnect();
//     Serial.println("Disconnected...");
//     WiFi.end();
//     Serial.println("Ended...");    
//   }
// }


void requestData() {
  if (wifiClient.connect(server, 80)) {
    // Make a HTTP request:
    wifiClient.print("GET ");
    wifiClient.print("/data/2.5/weather?q=");
    wifiClient.print(weather_loc);
    wifiClient.print("&units=metric&appid=");

    // switch URL to API request below if user wants to use lat, lon for exact weather location.
    // wifiClient.print("https://api.openweathermap.org/data/3.0/onecall?lat={lat}&lon={lon}&exclude={part}&appid={API key}")
    // wifiClient.print("/data/3.0/onecall?lat=");
    // wifiClient.print(lat);
    // wifiClient.print("&lon=");
    // wifiClient.print(lon);
    // wifiClient.print("&exclude={part}");
    // wifiClient.print("&appid=");
    wifiClient.print(appid);
    wifiClient.println(" HTTP/1.0");
    wifiClient.println("Host: api.openweathermap.org");
    wifiClient.println("Connection: close");
    wifiClient.println();
  }
}

void parseData() {
  static String line = "";
  while (wifiClient.connected()) {
    line = wifiClient.readStringUntil('\n');
    wifiClient.println(line);
    JSONVar myObject = JSON.parse(line);
    Serial.println("Openweather: " +String(line));
 
    // weather = JSON.stringify(myObject["weather"][0]["main"]);
    // weather.replace("\"", "");
    // town = JSON.stringify(myObject["name"]);
    // town.replace("\"", "");      
    temp_out = int(myObject["main"]["temp"]);
    windSpeed = int(myObject["wind"]["speed"]);
    
    if (line.startsWith("{")) {
       break;
    }
  }  
}

// void showWeather(String town, String weather) {
//   carrier.display.fillScreen(ST77XX_BLUE);
//   carrier.display.setCursor(60, 50);
//   carrier.display.setTextColor(ST77XX_WHITE);
//   carrier.display.setTextSize(3);
//   carrier.display.print(town);
//   carrier.display.setCursor(50, 110);
//   carrier.display.setTextSize(4);  
//   carrier.display.print(weather);
// }
