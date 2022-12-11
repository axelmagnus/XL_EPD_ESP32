/***************************************************
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include "Adafruit_ThinkInk.h"
#include "Adafruit_GFX.h"

#define SRAM_CS 32
#define EPD_CS 15
#define EPD_DC 33  

#define EPD_RESET   -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)

// Uncomment the following line if you are using 2.13" Monochrome EPD with SSD1680
ThinkInk_213_Mono_BN display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
//ThinkInk_213_Mono_B74 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);


// Uncomment the following line if you are using 2.13" EPD with SSD1675
// ThinkInk_213_Mono_B72 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// Uncomment the following line if you are using 2.13" EPD with SSD1675B
// ThinkInk_213_Mono_B73 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// set up the 'time/ISO-8601' topic
AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);

#define COLOR1 EPD_BLACK
#define COLOR2 EPD_RED

float temp=19.7;
// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Serial.println("2.13 inch EInk Featherwing test");
  iso->onMessage(handleISO);

  display.begin();
  // large block of text
  display.clearBuffer();
  display.setCursor(10, 10);
  display.setTextSize(3)
  display.print(temp,1);
  display.setCursor(30, 10);
  display.setTextSize(2)
  display.print(hum, 1);

  display.display();


  display.clearBuffer();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, COLOR1);
  }

  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, COLOR2);  // on grayscale this will be mid-gray
  }
  display.display();

}

void loop() {
  //don't do anything!
}


void testdrawtext(const char *text, uint16_t color) {
  display.setCursor(0, 0);
  display.setTextColor(color);
  display.setTextWrap(true);
  display.print(text);
}

// message handler for the ISO-8601 feed
void handleISO(char *data, uint16_t len)
{
  Serial.print("ISO Feed: ");
  Serial.println(data);
  int splitT = data.indexOf("T");
  dayStamp = data.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = data.substring(splitT + 1, data.length() - 1);
  Serial.println(timeStamp);
}