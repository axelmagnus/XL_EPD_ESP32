#include "/Users/axelmansson/Documents/Arduino/libraries/QRCode/src/qrcode.h"
#include <string>
#include "Adafruit_ThinkInk.h"
#include "Adafruit_GFX.h"
#include "Adafruit_LC709203F.h"
#include <SPI.h>
#include <Adafruit_AHTX0.h>
#include "config.h"
//#include <Fonts/FreeMonoBoldOblique18pt7b.h> 

#define SRAM_CS 32
#define EPD_CS 15
#define EPD_DC 33  
#define EPD_RESET   -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)
#define COLOR1 EPD_BLACK
#define Lcd_X 255
#define Lcd_Y 122

// Uncomment the following line if you are using 2.13" Monochrome EPD with SSD1680
ThinkInk_213_Mono_BN display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
/* //ThinkInk_213_Mono_B74 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);


// Uncomment the following line if you are using 2.13" EPD with SSD1675
// ThinkInk_213_Mono_B72 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Uncomment the following line if you are using 2.13" EPD with SSD1675B
// ThinkInk_213_Mono_B73 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
 */
// set up the 'time/ISO-8601' topic
AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);
Adafruit_LC709203F lc;
Adafruit_AHTX0 aht;
QRCode qrcode;
AdafruitIO_Group *group = io.group("ESP32S2TFT");

const int QRcode_Version = 4; //  set the version (range 1->40)
const int QRcode_ECC = 0;     //  set the Error Correction level (range 0-3) or symbolic (ECC_LOW, ECC_MEDIUM, ECC_QUARTILE and ECC_HIGH)

float temptemp=19.7;
sensors_event_t humidity, temp; // global for show foo
String formattedDate;
String dayStamp;
String timeStamp;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  // Allocate memory to store the QR code.
  // memory size depends on version number
  uint8_t qrcodeData[qrcode_getBufferSize(QRcode_Version)];
  qrcode_initText(&qrcode, qrcodeData, QRcode_Version, QRcode_ECC, "https://io.adafruit.com/axelmagnus/dashboards/esp32s2tft?kiosk=true");

  pinMode(LED_BUILTIN, OUTPUT);

  //pinMode(TFT_I2C_POWER, OUTPUT);
  /* delay(1);
  bool polarity = digitalRead(TFT_I2C_POWER);
  // Serial.println(polarity); =0 usually
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, !polarity);
   */
  //digitalWrite(TFT_I2C_POWER, HIGH);
  // initialize TFT
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
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER){ // woken upp by timer; send data, do not start TFT, sleep
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
        // tft.println("wifi failed, sleeping for 6 secs");
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
  }else{ // ext/RST wakeup, show TFT
    Serial.println("Visa på screen, klicka eller sov 6");

    display.begin();
    // large block of text
    display.clearBuffer();
    display.setCursor(10, 10);
    display.setTextSize(3);
    display.print(temp.temperature, 1);
    display.setCursor(30, 10);
    display.setTextSize(2);
    display.print(humidity.relative_humidity, 1);
    display.display();

    display.clearBuffer();
    for (int16_t i = 0; i < display.width(); i += 4)
    {
      display.drawLine(0, 0, i, display.height() - 1, COLOR1);
    }

    for (int16_t i = 0; i < display.height(); i += 4)
    {
      display.drawLine(display.width() - 1, 0, 0, i, COLOR1); // on grayscale this will be mid-gray
    }
    display.display();
  }
}

void loop() {
  //don't do anything!
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
