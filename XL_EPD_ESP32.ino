// For ESP32S2 with 2.13 " eink monchrome and Si7021 temp sensor
#include <Adafruit_Si7021.h>
#include "qrcode.h"
// #include <string>
#include "Adafruit_ThinkInk.h"
#include "Adafruit_GFX.h"
#include "Adafruit_LC709203F.h"
#include <SPI.h>
#include <Adafruit_AHTX0.h>
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

// version 3 code with double sized code and starting at y0 = 2 is good
// version 3 with ECC_LOW gives 53 "bytes". Size= (QRcode_Version*4 +17)*pixelsize =37*3=111
// The version of a QR code is a number between 1 and 40 (inclusive), which indicates the size of the QR code.

int pixelsize = 3;
const int QRcode_Version = 5; //  set the version (range 1->40)
const int QRcode_ECC = 0;     //  set the Error Correction level (range 0-3) or symbolic (ECC_LOW, ECC_MEDIUM, ECC_QUARTILE and ECC_HIGH)

uint8_t QRx0 = 135; // Width= 116
uint8_t QRy0 = 4;   // Where to start the QR pic

// For ISO time feed
String month;
int minute;
int hour;
int day;
bool got_time = false; // for making sure the time has been received before sleep

void setup()
{
  Serial.begin(115200);
/*   while(!Serial){
    delay(10);
  } */
  Serial.println("Serial ready");

  // Allocate memory to store the QR code.
  // memory size depends on version number
  uint8_t qrcodeData[qrcode_getBufferSize(QRcode_Version)];
  qrcode_initText(&qrcode, qrcodeData, QRcode_Version, QRcode_ECC, "https://io.adafruit.com/axelmagnus/dashboards/battlevel?kiosk=true");

  pinMode(LED_BUILTIN, OUTPUT);
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  pinMode(PIN_I2C_POWER, INPUT);
  bool polarity = digitalRead(PIN_I2C_POWER);
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, 0); // turn on I2C
#endif

  delay(200);
  iso->onMessage(handleISO);

  if (!lc.begin())
  {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
  }
  Serial.println(F("Found LC709203F"));
  lc.setThermistorB(3950);
  lc.setPackSize(LC709203F_APA_100MAH);
  lc.setAlarmVoltage(3.8);

  if (!sensor.begin())
  {
    Serial.println("Did not find Si7021 sensor!");
  }

  // Serial.println(F("Initialized"));
  // if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER){ // woken upp by timer; send data, do not start display, sleep
  // Serial.println("Wakeup caused by timer");
  digitalWrite(LED_BUILTIN, 1);
  io.connect();
  int wifiretries = 0;
  int led = 1;
  while (io.status() < AIO_CONNECTED)
  {
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
      // display.println("wifi failed, sleeping for 6 secs");
      Serial.println("wifi failed, sleeping for 6 secs");
      Serial.flush();
      esp_sleep_enable_timer_wakeup(6000000); // sleep for a minute if wifi fails
      esp_deep_sleep_start();
    }
  }
  Serial.print("wifi done ");
  Serial.println(sensor.readTemperature(), 1);
  Serial.println(io.statusText());
  digitalWrite(LED_BUILTIN, 0);
  io.run();
  group->set("Percent", lc.cellPercent());
  group->set("Voltage", lc.cellVoltage());
  group->set("Temp", sensor.readTemperature());
  group->set("Hum", sensor.readHumidity());
  group->save();

  digitalWrite(LED_BUILTIN, 1);
  delay(100);
  digitalWrite(LED_BUILTIN, 0);
  delay(100);
  digitalWrite(LED_BUILTIN, 1);
  delay(100);
  digitalWrite(LED_BUILTIN, 0);
  delay(100);
  Serial.println("iorun");
  while (!got_time) // Wait for ISO time, || !got_time
  {
    Serial.println(io.run());
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    delay(100);
  }
  /* }
   else
   { // ext/RST wakeup, show display
   */
  Serial.println("io.run done");
  Serial.println(month);

  display.begin();
  display.clearBuffer();
  display.setTextColor(EPD_BLACK);
  display.setFont(&FreeSerifBold24pt7b);
  display.setCursor(3, 37);
  display.print(sensor.readTemperature(), 1);
  display.setFont(&FreeSans9pt7b);
  display.setTextSize(1);
  display.setCursor(88, 15);
  display.println("o");
  display.setCursor(3, 58);
  display.print("Fukt: ");
  display.print(sensor.readHumidity(), 0);
  display.println(" %");
  // display.setCursor(3, 70);
  // display.setTextSize(2);
  //  display.print(humidity.relative_humidity, 1);
  display.setCursor(2, 117);
  // display.setTextColor(EPD_RED);
  display.print(day);
  display.print(" ");
  display.print(month);
  display.print(" ");
  if (hour < 10)
    display.print(" "); // if only one digit
  display.print(hour);
  display.print(":");
  if (minute < 10)
    display.print("0"); // if only one digit
  display.print(minute);
  display.print(" ");
  display.setCursor(3, 72);
  display.setFont();
  display.setTextColor(EPD_BLACK);
  display.print(lc.cellVoltage(), 2);
  display.print(" V ");
  display.print(lc.cellPercent(), 1);
  display.print(" %");

  display.setTextSize(1);
  display.setCursor(3, 90);
  display.println("Last updated:");

  //--------------------------------------------
  // display QRcode
  for (uint8_t y = 0; y < qrcode.size; y++)
  {
    for (uint8_t x = 0; x < qrcode.size; x++)
    {

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
        /*
        display.drawPixel(x0 + 3 * x, y0 + 3 * y, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y, EPD_WHITE);
        // display.drawPixel(x0 + 3 * x + 3, y0 + 3 * y, EPD_WHITE);

        display.drawPixel(x0 + 3 * x, y0 + 3 * y + 1, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y + 1, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y + 1, EPD_WHITE);
        // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 1, EPD_WHITE);

        display.drawPixel(x0 + 3 * x, y0 + 3 * y + 2, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 1, y0 + 3 * y + 2, EPD_WHITE);
        display.drawPixel(x0 + 3 * x + 2, y0 + 3 * y + 2, EPD_WHITE);
        // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 2, EPD_WHITE);
        */

        // display.drawPixel(x0 + 4 * x, y0 + 4 * y + 3, EPD_WHITE);
        // display.drawPixel(x0 + 4 * x + 1, y0 + 4 * y + 3, EPD_WHITE);
        // display.drawPixel(x0 + 4 * x + 2, y0 + 4 * y + 3, EPD_WHITE);
        // display.drawPixel(x0 + 4 * x + 3, y0 + 4 * y + 3, EPD_WHITE);
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
  io.wifi_disconnect();//release IP address?
  delay(100);                              // to let EPD settle
  digitalWrite(PIN_I2C_POWER, HIGH);        // Turn off I2C, necessary? PD_config?

  esp_sleep_enable_timer_wakeup(600000000); // 600  seconds to start with. sleep ten minutes
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
  struct tm tm = {0};
  // Convert to tm struct
  strptime(data, "%Y-%m-%dT%H:%M:%SZ", &tm);
  Serial.print("tm: ");
  Serial.println(&tm);
  Serial.println(tm.tm_mon);
  switch (tm.tm_mon){
  case 0:
    month = "jan";
    break;
  case 1:
    month = "feb";
    break;
  case 2:
    month = "mar";
    break;
  case 3:
    month = "apr";
    break;
  case 4:
    month = "may";
    break;
  case 5:
    month = "jun";
    break;
  case 6:
    month = "jul";
    break;
  case 7:
    month = "aug";
    break;
  case 8:
    month = "sep";
    break;
  case 9:
    month = "oct";
    break;
  case 10:
    month = "nov";
    break;
  case 11:
    month = "dec";
    break;
  }
  day = tm.tm_mday;
  hour = tm.tm_hour + 1; // TZ
  minute = tm.tm_min;
  Serial.println(month);

  got_time = true;
}
