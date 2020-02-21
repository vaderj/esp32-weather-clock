//Much of this originates from multiple projects by https://github.com/witnessmenow
//I can't remember where I got the http query of openweathermap


// Creates a second buffer for backround drawing (doubles the required RAM)
#define PxMATRIX_double_buffer true
//

#include <ezTime.h>
#include <PxMatrix.h>
#include <Fonts/FreeMono9pt7b.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

//Create a tasks to help manage the load between both cores
TaskHandle_t Task1, Task2;

// Initialize Wifi connection to the router

char ssid[] = "MySSID";     // your network SSID (name)
char password[] = "p@s$w0rD!"; // your network key
char hostname[] = "My_ESP32_Weather_Clock"; // your hostname

// OpenWeatherMap.org authentication
const String endpoint = "http://api.openweathermap.org/data/2.5/weather?zip=97222,us&units=imperial&APPID=" ;
const String apiKey = "abcdefghijklmnopqurstuvwxyz01234" ;

// Set a timezone using the following list
// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
#define MYTIMEZONE "America/Los_Angeles"

// If this is set to false, the number will only change if the value behind it changes
// e.g. the digit representing the least significant minute will be replaced every minute,
// but the most significant number will only be replaced every 10 minutes.
// When true, all digits will be replaced every minute.
bool forceRefresh = true;
// -----------------------------

// Pins for LED MATRIX
#ifdef ESP32
#include <WiFi.h>
#define P_LAT 22
#define P_A 19
#define P_B 23
#define P_C 18
#define P_D 5
#define P_E 15
#define P_OE 2
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <Ticker.h>
Ticker display_ticker;
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2
#endif

#define matrix_width 64
#define matrix_height 16

Timezone myTZ;

String currentTimestamp = "" ;
String thisHour = "" ;
String thisMinute = "" ;
String thisSecond = "" ;
String thisMilli = "" ;
String amPm = "" ;
bool ranThisMin = "" ;
String currentMessage = "" ;

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time = 20; //10-50 is usually fine

PxMATRIX display(64, 16, P_LAT, P_OE, P_A, P_B, P_C);
//PxMATRIX display(64,16,P_LAT, P_OE,P_A,P_B,P_C,P_D);
//PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);
//PxMATRIX display(64,64,P_LAT, P_OE,P_A,P_B,P_C,P_D,P_E);

// Some standard colors
uint16_t myRED = display.color565(255, 0, 0);
uint16_t myRED4 = display.color565(255, 0, 0);
uint16_t myRED3 = display.color565(192, 0, 0);
uint16_t myRED2 = display.color565(128, 0, 0);
uint16_t myRED1 = display.color565(64, 0, 0);
uint16_t myRED0 = display.color565(0, 0, 0);

uint16_t myCYAN4 = display.color565(0, 255, 255);
uint16_t myCYAN3 = display.color565(0, 192, 192);
uint16_t myCYAN2 = display.color565(0, 128, 128);
uint16_t myCYAN1 = display.color565(0, 64, 64);
uint16_t myCYAN0 = display.color565(0, 0, 0);

uint16_t currentHourColor = myRED ;
uint16_t currentMinuteColor = myRED ;
uint16_t currentSecondColor = myRED ;
uint16_t myCYAN = display.color565(0, 255, 255);
uint16_t currentAmPmColor = myCYAN ;

uint16_t myORANGE = display.color565(255, 128, 0);
uint16_t myYELLOW = display.color565(255, 255, 0);
uint16_t myGREEN = display.color565(0, 255, 0);
uint16_t myLIGHTBLUE = display.color565(128, 255, 255);
uint16_t myBLUE = display.color565(0, 0, 255);
uint16_t myBLUE1 = display.color565(0, 64, 255);
uint16_t myBLUE2 = display.color565(31, 127, 255);

uint16_t myBnfORANGE = display.color565(255, 100, 30);

uint16_t myMAGENTA = display.color565(255, 0, 255);
uint16_t myBLACK = display.color565(0, 0, 0);
uint16_t myWHITE = display.color565(255, 255, 255);

uint16_t myDEFAULT = display.color565(96, 96, 200);

uint16_t colon0Color1;
uint16_t colon0Color2;
uint16_t colon0Color3;
uint16_t colon0Color4;


unsigned long last_draw=0;


#ifdef ESP8266
// ISR for display refresh
void display_updater()
{
  display.display(display_draw_time);
}
#endif

#ifdef ESP32
void IRAM_ATTR display_updater() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  display.display(display_draw_time);
  portEXIT_CRITICAL_ISR(&timerMux);
}
#endif

union single_double {
  uint8_t two[2];
  uint16_t one;
} this_single_double;


void display_update_enable(bool is_enable)
{

#ifdef ESP8266
  if (is_enable)
    display_ticker.attach(0.002, display_updater);
  else
    display_ticker.detach();
#endif

#ifdef ESP32
  if (is_enable)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &display_updater, true);
    timerAlarmWrite(timer, 2000, true);
    timerAlarmEnable(timer);
  }
  else
  {
    timerDetachInterrupt(timer);
    timerAlarmDisable(timer);
  }
#endif
}


void setup() {
  Serial.begin(19200);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);

  // ESP32 : Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // END ESP32

  // Setup EZ Time
  setDebug(INFO);
  waitForSync();

  Serial.println();
  Serial.println("UTC:             " + UTC.dateTime());

  myTZ.setLocation(F(MYTIMEZONE));
  Serial.print(F("Time in your set timezone:         "));
  Serial.println(myTZ.dateTime());

  // Define your display layout here, e.g. 1/8 step, and optional SPI pins begin(row_pattern, CLK, MOSI, MISO, SS)
  display.begin(8);
  display.setBrightness(75);

  display.setFastUpdate(true);
  display.clearDisplay();

  display_update_enable(true);
  delay(5000);

  
 xTaskCreatePinnedToCore(
    codeForTask1,
    "clock1Task",
    10000,
    NULL,
    1,
    &Task1,
    0);
  delay(500);  // needed to start-up task1

  xTaskCreatePinnedToCore(
    codeForTask2,
    "clock2Task",
    10000,
    NULL,
    1,
    &Task2,
    1);
}


void setColonColor(String thisMilli){
  if (thisMilli.toInt() >= 0 && thisMilli.toInt() < 125) {
      colon0Color1 = myGREEN;
      colon0Color2 = myBLACK;
      colon0Color3 = myBLACK;
      colon0Color4 = myBLACK;
    }
    else if (thisMilli.toInt() >= 125 && thisMilli.toInt() < 250) {
      colon0Color1 = myBLACK;
      colon0Color2 = myGREEN;
      colon0Color3 = myBLACK;
      colon0Color4 = myBLACK;
    }
    else if (thisMilli.toInt() >= 250 && thisMilli.toInt() < 375) {
      colon0Color1 = myBLACK;
      colon0Color2 = myBLACK;
      colon0Color3 = myGREEN;
      colon0Color4 = myBLACK;
    }

    else if (thisMilli.toInt() >= 375 && thisMilli.toInt() < 500) {
      colon0Color1 = myBLACK;
      colon0Color2 = myBLACK;
      colon0Color3 = myBLACK;
      colon0Color4 = myGREEN;
    }

    else if (thisMilli.toInt() >= 500 && thisMilli.toInt() < 625) {
      colon0Color1 = myGREEN;
      colon0Color2 = myBLACK;
      colon0Color3 = myBLACK;
      colon0Color4 = myBLACK;
    }

    else if (thisMilli.toInt() >= 625 && thisMilli.toInt() < 750) {
      colon0Color1 = myBLACK;
      colon0Color2 = myGREEN;
      colon0Color3 = myBLACK;
      colon0Color4 = myBLACK;
    }

    else if (thisMilli.toInt() >= 750 && thisMilli.toInt() < 875) {
      colon0Color1 = myBLACK;
      colon0Color2 = myBLACK;
      colon0Color3 = myGREEN;
      colon0Color4 = myBLACK;
    }

    else if (thisMilli.toInt() >= 875 && thisMilli.toInt() < 1001) {
      colon0Color1 = myBLACK;
      colon0Color2 = myBLACK;
      colon0Color3 = myBLACK;
      colon0Color4 = myGREEN;
    }
}

void scroll_text(uint8_t ypos, unsigned long scroll_delay, String text, uint8_t colorR, uint8_t colorG, uint8_t colorB)
{
  uint16_t text_length = text.length();
  display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
  display.setTextSize(1);
  display.setRotation(0);
  display.setTextColor(display.color565(colorR,colorG,colorB));

  // Asuming 5 pixel average character width
  for (int xpos=matrix_width; xpos>-(matrix_width+text_length*5); xpos--)
  {
    display.setTextColor(display.color565(colorR,colorG,colorB));
    display.setCursor(xpos,ypos);
    display.println(text);
    delay(scroll_delay);
    yield();
  }
}

void drawTime(){
    thisHour = myTZ.dateTime("g") ;
    thisMinute = myTZ.dateTime("i") ;
    thisSecond = myTZ.dateTime("s") ;
    thisMilli = myTZ.dateTime("v") ;
    amPm = myTZ.dateTime("A") ;
    currentTimestamp = myTZ.dateTime();
 
    setColonColor(thisMilli);

//  Clock
  // If the time is between 6pm and 6am, dim the panel to 15% brightness
    if (amPm == "AM" && thisHour.toInt() < 6) {
      //      Serial.println("AM < 6");
      display_draw_time = 30;
      display.setBrightness(12);
    }
    else if (amPm == "PM" && thisHour.toInt() >= 6) {
      //      Serial.println("PM >= 6");
      display_draw_time = 30;
      display.setBrightness(12);
    }
    else {
      display_draw_time = 20;
      display.setBrightness(75);
    }

//  Hour
    if ( thisHour.toInt() == 11 ) {
      display.setCursor(2, 8);
    }
    else if (thisHour.toInt() == 10 || thisHour.toInt() == 12) {
      display.setCursor(0, 8);
    }
    else if (thisHour.toInt() == 1) {
      display.setCursor(7, 8);
    }
    else {
      display.setCursor(5, 8);
    }
  
    display.setTextColor(myRED);
    display.print(thisHour);

// Minutes Colon
  if(thisHour.toInt() > 9 ||  thisHour.toInt() == 1){
    display.drawPixel(15, 10, colon0Color1);
    display.drawPixel(14, 10, colon0Color2);
    display.drawPixel(14, 9, colon0Color3);
    display.drawPixel(15, 9, colon0Color4);

    display.drawPixel(15, 12, colon0Color1);
    display.drawPixel(14, 12, colon0Color2);
    display.drawPixel(14, 13, colon0Color3);
    display.drawPixel(15, 13, colon0Color4);
  }
  else{
    display.drawPixel(14, 10, colon0Color1);
    display.drawPixel(13, 10, colon0Color2);
    display.drawPixel(13, 9, colon0Color3);
    display.drawPixel(14, 9, colon0Color4);

    display.drawPixel(14, 12, colon0Color1);
    display.drawPixel(13, 12, colon0Color2);
    display.drawPixel(13, 13, colon0Color3);
    display.drawPixel(14, 13, colon0Color4);
  }
    
//  Minute
    display.setTextColor(myRED);
    display.setCursor(18, 8);
    display.print(thisMinute);

//  Second colon
    //Top dot
    display.drawPixel(32, 10, colon0Color1);
    display.drawPixel(33, 10, colon0Color2);
    display.drawPixel(33, 9, colon0Color3);
    display.drawPixel(32, 9, colon0Color4);

//Bottom dot
    display.drawPixel(32, 12, colon0Color1);
    display.drawPixel(33, 12, colon0Color2);
    display.drawPixel(33, 13, colon0Color3);
    display.drawPixel(32, 13, colon0Color4);

// Second
    display.setTextColor(myRED);
    display.setCursor(37, 8);
    display.print(thisSecond);

//  AmPm    
    display.setTextColor(myCYAN);
    display.setCursor(52, 8);
    display.print(amPm);
    yield();  
}


void getWeather(){
  if( thisSecond.toInt() == 59){
    ranThisMin = false ;
    Serial.print("Setting ranThisMin to false  -");
    delay(100);
    return;
  }
  else if (ranThisMin == true){
    //If the weather has already been fetched this hour, skip
    return ;
  }
  else if(thisSecond.toInt() == 0 && ranThisMin == false && thisMinute.toInt() % 2){
    HTTPClient http;
    http.begin(endpoint + apiKey); //Specify the URL
    int httpCode = http.GET();  //Make the request
 
    if (httpCode > 0) { //Check for the returning code
        String payload = http.getString();
        
        const size_t capacity = JSON_ARRAY_SIZE(2) + 2*JSON_OBJECT_SIZE(1) + 2*JSON_OBJECT_SIZE(2) + 2*JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(14) + 330;
        DynamicJsonDocument doc(capacity);
        int str_len = payload.length() + 1; 
        char jsonPaylod[str_len];
        payload.toCharArray(jsonPaylod, str_len);
        deserializeJson(doc, jsonPaylod);

        JsonObject weather_0 = doc["weather"][0];
        const char* weather_0_main = weather_0["main"]; // "Rain"
        const char* weather_0_description = weather_0["description"]; // "light rain"
        const char* weather_0_icon = weather_0["icon"]; // "10d"

        JsonObject main = doc["main"];
        int main_temp = (int)main["temp"];
        int main_feels_like = (int)main["feels_like"]; 
        int main_temp_min = (int)main["temp_min"];
        int main_temp_max = (int)main["temp_max"]; 
        int main_pressure = (int)main["pressure"];
        int main_humidity = (int)main["humidity"];
        int visibility = doc["visibility"]; 
        float wind_speed = doc["wind"]["speed"]; 
        int wind_deg = doc["wind"]["deg"]; 

        Serial.println("");
        Serial.println("");

        Serial.print("currentTimestamp : ");
        Serial.println(currentTimestamp);
        Serial.print("weather_0_main value :  " );
        Serial.println(weather_0_main);
        
        //Serial.println("");
        Serial.print("weather_0_description value :  " );
        Serial.println(weather_0_description);

        //Serial.println("");
        Serial.print("weather_0_icon value :  " );
        Serial.println(weather_0_icon);

        //Serial.println("");
        Serial.print("main_temp value :  " );
        Serial.println(main_temp);

        //Serial.println("");
        Serial.print("main_feels_like value :  " );
        Serial.println(main_feels_like);
        
        //Serial.println("");
        Serial.print("main_temp_min value :  " );
        Serial.println(main_temp_min);
        
        //Serial.println("");
        Serial.print("main_temp_max value :  " );
        Serial.println(main_temp_max);
        
        //Serial.println("");        
        Serial.print("main_pressure value :  " );
        Serial.println(main_pressure);

        //Serial.println("");
        Serial.print("main_humidity value :  " );
        Serial.println(main_humidity);
        
        //Serial.println("");
        Serial.print("wind_speed value :  " );
        Serial.println(wind_speed);
        
        //Serial.println("");
        Serial.print("wind_deg value :  " );
        Serial.println(wind_deg);
        Serial.println("");

        currentMessage = "Current temperature: " + String(main_temp) + "f           Current conditions: " + String(weather_0_description) + "           Todays low: " + String(main_temp_min) + "f           Todays high: " + main_temp_max + "f           ";
      }
    else {
      Serial.println("Error on HTTP request");
    }
    http.end(); //Free the resources
    ranThisMin = true ;
  }
}

void codeForTask1( void * parameter )
{
 while(true){
    delay(500);
//  Turns out I didn't end up using this task because of timing issues.
  }
}

void codeForTask2( void * parameter )
{
  while(true){   
    getWeather() ;
    delay(100);
  }
}

void loop() {
  int thisDelay = 175 ;
  int pixelsPerFrame = 4;
  display.setCursor(0, 0);

  if(currentMessage){
    uint16_t text_length = currentMessage.length();
    int colorR = 96;
    int colorG = 96;
    int colorB = 250;
    
    int scroll_delay = 60;
    int ypos = 0;
     
    display.setTextWrap(false);  // we don't wrap text so it scrolls nicely
    display.setTextSize(1);
    display.setRotation(0);
    display.setTextColor(display.color565(colorR,colorG,colorB));
  
    // Asuming 5 pixel average character width
    for (int xpos=matrix_width; xpos>-(matrix_width+text_length*5); xpos--)
    {
      display.clearDisplay() ;
      display.setTextColor(display.color565(colorR,colorG,colorB));
      display.setCursor(xpos,ypos);
      display.println(currentMessage);
      delay(scroll_delay);
      yield();
      drawTime();
      display.showBuffer() ;
    }
  }
    
  display_update_enable(true);
  delay(5);
  events();
}
