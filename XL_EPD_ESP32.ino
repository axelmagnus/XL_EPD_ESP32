// #include "/Users/axelmansson/Documents/Arduino/libraries/QRCode/src/qrcode.h"
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

#define SRAM_CS 32
#define EPD_CS 15
#define EPD_DC 33
#define EPD_RESET -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY -1  // can set to -1 to not use a pin (will wait a fixed delay)
#define COLOR1 EPD_BLACK
#define COLOR2 EPD_RED
#define Lcd_X 212
#define Lcd_Y 104

// Uncomment the following line if you are using 2.13" EPD with IL0373
ThinkInk_213_Tricolor_Z16 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);
Adafruit_LC709203F lc;
Adafruit_AHTX0 aht;
QRCode qrcode;
AdafruitIO_Group *group = io.group("ESP32EPD");

// version 3 code with double sized code and starting at y0 = 2 is good
// version 3 with ECC_LOW gives 53 "bytes". Size= (QRcode_Version*4 +17)*pixelsize =132
// The version of a QR code is a number between 1 and 40 (inclusive), which indicates the size of the QR code.

int pixelsize = 3;
const int QRcode_Version = 4; //  set the version (range 1->40)
const int QRcode_ECC = 0;     //  set the Error Correction level (range 0-3) or symbolic (ECC_LOW, ECC_MEDIUM, ECC_QUARTILE and ECC_HIGH)

float temptemp = 19.7;
sensors_event_t humidity, temp; // global for show foo
String formattedDate;
String dayStamp;
String timeStamp;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }
  Serial.print("Starting");

  // Allocate memory to store the QR code.
  // memory size depends on version number
  uint8_t qrcodeData[qrcode_getBufferSize(QRcode_Version)];
  qrcode_initText(&qrcode, qrcodeData, QRcode_Version, QRcode_ECC, "https://io.adafruit.com/axelmagnus/dashboards/esp32s2epd?kiosk=true");

  pinMode(LED_BUILTIN, OUTPUT);

  // pinMode(display_I2C_POWER, OUTPUT);
  /* delay(1);
  bool polarity = digitalRead(display_I2C_POWER);
  // Serial.println(polarity); =0 usually
  pinMode(display_I2C_POWER, OUTPUT);
  digitalWrite(display_I2C_POWER, !polarity);
   */
  // digitalWrite(display_I2C_POWER, HIGH);
  //  initialize display
  delay(50);
  esp_sleep_enable_timer_wakeup(600000000); // 600  seconds to start with. sleep ten minutes
  Serial.println(esp_sleep_get_wakeup_cause());

  iso->onMessage(handleISO);
  if (!lc.begin())
  {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));
  }
  // Serial.println(F("Found LC709203F"));
  lc.setThermistorB(3950);
  lc.setPackSize(LC709203F_APA_100MAH);
  lc.setAlarmVoltage(3.8);

  if (!aht.begin())
  {
    Serial.println("Could not find AHT? Check wiring");
  }
  aht.getEvent(&humidity, &temp); // populate temp and

  // Serial.println(F("Initialized"));
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER)
  { // woken upp by timer; send data, do not start display, sleep
    Serial.println("Wakeup caused by timer");
    digitalWrite(LED_BUILTIN, 1);
    io.connect();
    int wifiretries = 0; /*
     Serial.println("Starting wifi");
     Serial.println(WIFI_SSID);
     Serial.println(WIFI_PASS); */
    int led = 1;
    while (io.status() < AIO_CONNECTED)
    {
      led = !led;
      digitalWrite(LED_BUILTIN, led);
      wifiretries++;
      if (wifiretries < 59)
      {
        Serial.println(io.statusText());
        delay(500);
      }
      else // wifi failed, sleep short time
      {
        // display.println("wifi failed, sleeping for 6 secs");
        Serial.println("wifi failed, sleeping for 6 secs");
        esp_sleep_enable_timer_wakeup(6000000); // sleep for a minute if wifi fails
        esp_deep_sleep_start();
      }
    }
    io.run();
    group->set("Percent", lc.cellPercent());
    group->set("Voltage", lc.cellVoltage());
    group->set("Temp", temp.temperature);
    group->set("Hum", humidity.relative_humidity);
    group->save();
    Serial.print("Temp ");
    Serial.println(temp.temperature);
    digitalWrite(LED_BUILTIN, 1);
    delay(100);
    digitalWrite(LED_BUILTIN, 0);
    delay(100);
    digitalWrite(LED_BUILTIN, 1);
    delay(100);
    digitalWrite(LED_BUILTIN, 0);
    delay(100);
    io.run();
    Serial.println("iorun");
    Serial.print(io.run());
  }
  else
  { // ext/RST wakeup, show display
    Serial.println("Visa på screen, klicka eller sov 6");

    display.begin();
    display.clearBuffer();
    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeSerifBold24pt7b);
    display.setCursor(3, 37);
    //  display.print(temp.temperature, 1);
    display.print("21.4");
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setCursor(88, 15);
    display.println("o");
    display.setCursor(3, 55);
    // display.setTextSize(2);
    //  display.print(humidity.relative_humidity, 1);
    display.println("Fukt: 61 %");
    // display.println(63.2);
    // display.setCursor(3, 70);
    // display.setTextSize(2);
    //  display.print(humidity.relative_humidity, 1);
    display.setCursor(3, 96);
    // display.setTextColor(EPD_RED);
    display.println("09 apr 15:50");
    display.setCursor(3, 68);
    display.setFont();
    display.setTextSize(1);
    display.setTextColor(EPD_BLACK);
    display.println("3.72 V  21.2 %");
    display.setTextSize(1);
    display.setCursor(3, 72);
    display.println("Senast uppdaterad:");

    uint8_t x0 = 108; // Width= 106
    uint8_t y0 = 2;   // Where to start the QR pic
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
              display.drawPixel(x0 + pixelsize * x + j, y0 + pixelsize * y + i, EPD_WHITE);
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
              display.drawPixel(x0 + pixelsize * x + j, y0 + pixelsize * y + i, EPD_BLACK);
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
  /*
  int splitT = data.indexOf("T");
  dayStamp = data.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = data.substring(splitT + 1, data.length() - 1);
  Serial.println(timeStamp);
  */
}
