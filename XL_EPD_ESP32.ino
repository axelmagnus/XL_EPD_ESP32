// For ESP32S2 with 2.13 " eink monchrome and Si7021 temp sensor
#include <Adafruit_Si7021.h>
#include "qrcode.h"
// #include <string>
#include "Adafruit_ThinkInk.h"
#include "Adafruit_GFX.h"
#include "Adafruit_LC709203F.h"
#include <SPI.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_INA219.h>
#include "config.h"
#include <Fonts/FreeSerifBold24pt7b.h>
#include <Fonts/FreeMonoOblique12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// ESP32S2:
#define SRAM_CS 6
#define EPD_CS 9
#define EPD_DC 10
#define EPD_RESET -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY -1  // can set to -1 to not use a pin (will wait a fixed delay)
#define COLOR1 EPD_BLACK
#define COLOR2 EPD_RED
#define Lcd_X 250 // 2.13 w 1680; 4195
#define Lcd_Y 122
#define AIO_DEBUG

// Uncomment the following line if you are using 2.13" EPD with IL0373
// ThinkInk_213_Tricolor_Z16 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Uncomment the following line if you are using 2.13" Monochrome EPD with SSD1680
// ThinkInk_213_Mono_BN display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
ThinkInk_213_Mono_B72 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY); // working better?? seems to be SSD1675 chipset?

AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);
Adafruit_LC709203F lc;
Adafruit_Si7021 sensor = Adafruit_Si7021();
QRCode qrcode;
AdafruitIO_Group *group = io.group("ESP32S2");
Adafruit_INA219 ina219(0x41);

RTC_DATA_ATTR bool displaycharging; //0= display temp and feeds (on reset also), 1=display  INA,solar charger
// For ISO time feed
RTC_DATA_ATTR struct tm tm;
RTC_DATA_ATTR int month_int;
RTC_DATA_ATTR int minute;
RTC_DATA_ATTR int hour;
RTC_DATA_ATTR int day;
RTC_DATA_ATTR struct timeval sleep_enter_time;
RTC_DATA_ATTR long sleepTime; // in seconds
RTC_DATA_ATTR int wifiFail;   // count number of wifi fails

struct timeval now;
bool got_time = false; // for making sure the time has been received before sleep
// version 3 code with double sized code and starting at y0 = 2 is good
// version 3 with ECC_LOW gives 53 "bytes". Size= (QRcode_Version*4 +17)*pixelsize =37*3=111
// The version of a QR code is a number between 1 and 40 (inclusive), which indicates the size of the QR code.

int pixelsize = 3;
const int QRcode_Version = 5; //  set the version (range 1->40)
const int QRcode_ECC = 0;     //  set the Error Correction level (range 0-3) or symbolic (ECC_LOW, ECC_MEDIUM, ECC_QUARTILE and ECC_HIGH)

uint8_t QRx0 = 126; // Width= 116
uint8_t QRy0 = 1;   // Where to start the QR pic

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */

float busvoltage = 0;
float loadvoltage = 0;
float current_mA = 0;
float power_mW = 0;

void setup()
{
  Serial.begin(115200);
  /* while(!Serial){
      delay(10);
    } // wait for serial monitor to open */
  Serial.println("Serial ready");

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0); // enable ext0 wake up from pin 0
  // Allocate memory to store the QR code.
  // memory size depends on version number
  uint8_t qrcodeData[qrcode_getBufferSize(QRcode_Version)];
  qrcode_initText(&qrcode, qrcodeData, QRcode_Version, QRcode_ECC, "https://io.adafruit.com/axelmagnus/dashboards/esp32s2epd?kiosk=true");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, 0); // turn on I2C REV ?
  /* 
  The Feather ESP32-S2 also has a power 'switch' on the I2C port connected to GPIO 7 . On Rev B of the PCB, you have to set the pin LOW to turn on the I2C power and pullups. On Rev C of the PCB, you have to set the pin HIGH. */

  if (!lc.begin())
  {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
  }
  Serial.println(F("Found LC709203F"));
  lc.setThermistorB(3950);
  lc.setPackSize(LC709203F_APA_1000MAH);
  lc.setAlarmVoltage(3.3);

  if (!sensor.begin()){
    Serial.println("Did not find Si7021 sensor!");
  }
  
  ina219.begin()
      ? Serial.println("Found INA219")
      : Serial.println("Did not find INA219");
  ina219.setCalibration_16V_400mA();
  
  float shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  loadvoltage = busvoltage + (shuntvoltage / 1000);

  for (int i = 0; i < 1000; i++){
    //Serial.println(current_mA);
    current_mA += ina219.getCurrent_mA();
    power_mW += ina219.getPower_mW();
  }
  current_mA /= 1000; // average
  power_mW /= 1000;
  Serial.print("Current: ");
  Serial.print(current_mA);
  Serial.print(" mA, dsiplaycharging: ");
  Serial.print(displaycharging);
  Serial.println(" mW"); 
  digitalWrite(LED_BUILTIN, 1);


  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0){//update display  and go to sleep again
    displaycharging = !displaycharging;//toggle what to display
    displayData(displaycharging); // update display
    Serial.print("button wakeup.");
    if(sleepTime==0)sleepTime=600; //if not set with timer wake up yet
    // find  out how long youve slept and compensate sleeptime in esp_sleep_enable_timer_wakeup
    gettimeofday(&now, NULL);
    //Serial.println(&now);
    //Serial.println(&now-sleep_enter_time);
    long slept_time_s = (now.tv_sec - sleep_enter_time.tv_sec) ;//time slept in seconds
    // add these seconds to the tm object
   /*  //  convert struct tm to time_t
    time_t time = mktime(&t);

    // add seconds to time_t
    time += seconds_to_add;

    // convert time_t back to struct tm
    struct tm *new_t = localtime(&time); */
    Serial.print("slept_time_s: ");Serial.println(slept_time_s);
    sleepTime = sleepTime - slept_time_s;
    esp_sleep_enable_timer_wakeup(sleepTime * uS_TO_S_FACTOR);
    Serial.print("sleeptime: ");Serial.println(sleepTime);
    gettimeofday(&sleep_enter_time, NULL);
    esp_deep_sleep_start();
  } 
  //if not button press, go ahead and connect to internet

  iso->onMessage(handleISO);
  io.connect();

  int led;
  int wifiretries;
  while (io.status() < AIO_CONNECTED){
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    delay(50);
    wifiretries++;
    if (wifiretries < 59)
    {
      Serial.print("SSID: ");
      Serial.print(WIFI_SSID);
      Serial.print(" ");

      Serial.println(io.statusText());
      delay(500);
    }
    else // wifi failed, sleep short time
    {
      wifiFail++;
      Serial.println("wifi failed, sleeping for 6 secs");
      Serial.flush();
      digitalWrite(LED_BUILTIN, 0);
      esp_sleep_enable_timer_wakeup(6000000); // sleep for a minute if wifi fails
      esp_deep_sleep_start();
    }
  }
  Serial.print("wifi done ");
  Serial.println(io.statusText());
  digitalWrite(LED_BUILTIN, 0);
  io.run();
  group->set("Percent", lc.cellPercent());
  group->set("Voltage", lc.cellVoltage());
  group->set("Temp", sensor.readTemperature());
  group->set("Hum", sensor.readHumidity());
  group->set("Current", current_mA);
  group->set("Power", power_mW);
  group->set("WifiFail", wifiFail);
  group->save();

  digitalWrite(LED_BUILTIN, 1);
  delay(200);
  digitalWrite(LED_BUILTIN, 0);
  delay(100);
  digitalWrite(LED_BUILTIN, 1);
  delay(200);
  digitalWrite(LED_BUILTIN, 0);
  delay(100);
  
  Serial.println("iorun");
  long start = millis();
  while (!got_time && (millis() - start < 5000)){ // Wait for ISO time, for 5 seconds or got_time
    Serial.println(io.run());
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    delay(100);
  }
  Serial.println("io.run done");
  displaycharging=0;
  displayData(displaycharging); // update display with temp, not charging

  digitalWrite(LED_BUILTIN, 0);// be sure to turn off LED
  io.wifi_disconnect();                      // release IP address?
  delay(100);           // to let EPD settle
  // digitalWrite(PIN_I2C_POWER, HIGH);        // Turn off I2C, necessary? PD_config?
  sleepTime=600;//timer wake up; reset to 600 seconds 
  esp_sleep_enable_timer_wakeup(sleepTime*uS_TO_S_FACTOR); // 600  seconds to start with. sleep ten minutes
  gettimeofday(&sleep_enter_time, NULL);
  esp_deep_sleep_start();
}

void loop()
{
  // don't do anything!
}

// message handler for the ISO-8601 feed
void handleISO(char *data, uint16_t len)
{
  Serial.print("ISO Feed: ");
  Serial.println(data);
  // Convert to tm struct
  strptime(data, "%Y-%m-%dT%H:%M:%SZ", &tm);
  Serial.print("tm: ");
  Serial.println(&tm);
  Serial.println(tm.tm_mon);
  month_int=tm.tm_mon;//to store in RTC
  day = tm.tm_mday;
  hour = tm.tm_hour + 2; // TZ
  minute = tm.tm_min;
  Serial.println(month_int2string(month_int));

  got_time = true;//we can sleep, we have gotten the time
}
String month_int2string(int mon){//convert int of month to string:
  switch (mon)
  {
  case 0:
    return "Jan";
    break;
  case 1:
    return "Feb";
    break;
  case 2:
    return "Mar";
    break;
  case 3:
    return "Apr";
    break;
  case 4:
    return "May";
    break;
  case 5:
    return "Jun";
    break;
  case 6:
    return "Jul";
    break;
  case 7:
    return "Aug";
    break;
  case 8:
    return "Sep";
    break;
  case 9:
    return "Oct";
    break;
  case 10:
    return "Nov";
    break;
  case 11:
    return "Dec";
    break;
  default:
    return "Err";
    break;
  }
}

void displayData(bool displaycharger){
  Serial.println("update display, month_int:");
  Serial.println(month_int);
  display.begin();
  display.setRotation(2);
  display.clearBuffer();
  display.setTextColor(EPD_BLACK);
  if (displaycharger){ // show current
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setCursor(3, 13);
    display.print("Curr: ");
    display.print(current_mA, 0);
    display.println(" mA");
    display.setCursor(3, 33);
    display.print("P: ");
    display.print(power_mW, 0);
    display.println(" mW");
    display.setCursor(3, 52);
    display.print("Lvolt: ");
    display.print(loadvoltage, 2);
    display.println(" V");
  }
  else{ // show temp
    display.setFont(&FreeSerifBold24pt7b);
    display.setCursor(3, 39);
    display.print(sensor.readTemperature(), 1);
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setCursor(91, 17);
    display.println("o");
    display.setCursor(3, 60);
    display.print("Fukt: ");
    display.print(sensor.readHumidity(), 0);
    display.println(" %");
  }
  
  display.setFont();
  display.setCursor(3, 88);
  display.println("Last uploaded:");
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setCursor(3, 111);
  // display.setTextColor(EPD_RED);
  display.print(day);
  display.print(" ");
  display.print(month_int2string(month_int));
  display.print(" ");
  if (hour < 10)
    display.print(" "); // if only one digit
  display.print(hour);
  display.print(":");
  if (minute < 10)
    display.print("0"); // if only one digit
  display.print(minute);
  display.print(" ");
  display.setCursor(3, 75);
  display.setFont();
  display.setTextColor(EPD_BLACK);
  display.print(lc.cellVoltage(), 2);
  display.print(" V ");
  display.print(lc.cellPercent(), 1);
  display.print(" %");

  //--------------------------------------------
  // display QRcode
  for (uint8_t y = 0; y < qrcode.size; y++){
    for (uint8_t x = 0; x < qrcode.size; x++){

        if (qrcode_getModule(&qrcode, x, y) == 0)
        { // change to == 1 to make QR code with black background

          // uncomment to double the QRcode. Comment to display normal code size
          for (int i = 0; i < pixelsize; i++)
          {
            for (int j = 0; j < pixelsize; j++)
            {
              display.drawPixel(QRx0 + pixelsize * x + j, QRy0 + pixelsize * y + i, EPD_WHITE);
            }
          }
        }
        else
        {
          for (int i = 0; i < pixelsize; i++)
          {
            for (int j = 0; j < pixelsize; j++)
            {
              display.drawPixel(QRx0 + pixelsize * x + j, QRy0 + pixelsize * y + i, EPD_BLACK);
            }
          }
          /*
          display.drawPixel(x0 + 3 * x, y0 + 3 * y, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y, EPD_BLACK);
          // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y, EPD_BLACK);

          display.drawPixel(x0 + 3 * x, y0 + 3 * y + 1, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y + 1, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y + 1, EPD_BLACK);
          // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 1, EPD_BLACK);

          display.drawPixel(x0 + 3 * x, y0 + 3 * y + 2, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y + 2, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y + 2, EPD_BLACK);
          // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 2, EPD_BLACK);

          display.drawPixel(x0 + 3 * x, y0 + 3 * y + 3, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y + 3, EPD_BLACK);
          display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y + 3, EPD_BLACK);
          // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 3, EPD_BLACK);
          */
        }
    }
  }
  display.display();
}