// Example of drawing a graphical "switch" and using
// the touch screen to change it's state.

// This sketch does not use the libraries button drawing
// and handling functions.

// Based on Adafruit_GFX library onoffbutton example.

// Touch handling for XPT2046 based screens is handled by
// the TFT_eSPI library.

// Calibration data is stored in SPIFFS so we need to include it
#include "FS.h"

#include <SPI.h>

#include <TFT_eSPI.h> // Hardware-specific library

// Firebase
#include <WiFi.h>
#include <FirebaseESP32.h>

#define FIREBASE_HOST "HOST"
#define FIREBASE_AUTH "AUTH"
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

//Define FirebaseESP32 data object
FirebaseData fbdo;
FirebaseData fbdo_stream;
FirebaseJson json;

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

// This is the file name used to store the touch coordinate
// calibration data. Cahnge the name to start a new calibration.
#define CALIBRATION_FILE "/TouchCalData3"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

boolean SwitchOn = true;

// Comment out to stop drawing black spots
#define BLACK_SPOT

// Switch position and size 320x240
#define FRAME_X 0
#define FRAME_Y 0
#define FRAME_W 320
#define FRAME_H 240

// Red zone size
#define REDBUTTON_X FRAME_X
#define REDBUTTON_Y FRAME_Y
#define REDBUTTON_W (FRAME_W / 2)
#define REDBUTTON_H FRAME_H

// Green zone size
#define GREENBUTTON_X (REDBUTTON_X + REDBUTTON_W)
#define GREENBUTTON_Y FRAME_Y
#define GREENBUTTON_W (FRAME_W / 2)
#define GREENBUTTON_H FRAME_H

//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void setup(void)
{
  Serial.begin(9600);
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(1);

  // call screen calibration
  touch_calibrate();

  // clear screen
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextFont(0); // Select font 0 which is the Adafruit font

  // connect WIFI
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  tft.print("Connecting to Wi-Fi...");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  tft.setCursor(0, 10);
  tft.print("Connected with IP: ");
  tft.setCursor(0, 20);
  tft.print(WiFi.localIP());

  tft.setCursor(0, 30);
  tft.print("Connecting to Firebase...");

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // Get data from firebase
  if (Firebase.getBool(fbdo, "/SwitchOn/data"))
  {
    //Success
    Serial.print("Get bool data success, bool = ");
    Serial.println(fbdo.boolData());
    SwitchOn = fbdo.boolData() == 1 ? true : false;
  }
  else
  {
    //Failed?, get the error reason from fbdo
    Serial.print("Error in getbool, ");
    Serial.println(fbdo.errorReason());

    tft.setCursor(0, 70);
    tft.print("Error getBool SwitchOn/data: ");
    tft.setCursor(0, 80);
    tft.print(fbdo.errorReason());
  }

  //In setup(), set the streaming path to "/SwitchOn/data" and begin stream connection
  if (!Firebase.beginStream(fbdo_stream, "/SwitchOn/data"))
  {
    //Could not begin stream connection, then print out the error detail
    Serial.println(fbdo_stream.errorReason());
    tft.setCursor(0, 40);
    tft.print("Error beginStream SwitchOn/data: ");
    tft.setCursor(0, 50);
    tft.print(fbdo_stream.errorReason());
  }
  else
  {
    tft.setCursor(0, 40);
    tft.print("Firebase Streaming...");
  }

  //streamCallback is the function that called when database data changes or updates occurred
  //streamTimeoutCallback is the function that called when the connection between the server
  //and client was timeout during HTTP stream
  //Firebase.setStreamCallback(fbdo_stream, streamCallback, streamTimeoutCallback);

  // Draw button (this example does not use library Button class)
  if (SwitchOn)
  {
    greenBtn();
  }
  else
  {
    redBtn();
  }
}
//------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------
void loop()
{
  uint16_t x, y;

  // See if there's any touch data for us
  if (tft.getTouch(&x, &y))
  {
// Draw a block spot to show where touch was calculated to be
#ifdef BLACK_SPOT
    tft.fillCircle(x, y, 2, TFT_BLACK);
#endif

    if (SwitchOn)
    {
      if ((x > REDBUTTON_X) && (x < (REDBUTTON_X + REDBUTTON_W)))
      {
        if ((y > REDBUTTON_Y) && (y <= (REDBUTTON_Y + REDBUTTON_H)))
        {
          Serial.println("Red btn hit");
          //fbdo_stream.stopWiFiClient();
          // Firebase Update
          if (Firebase.setBool(fbdo, "/SwitchOn/data", false))
          {
            //Success
            Serial.println("setBool: false success");
            redBtn();
          }
          else
          {
            //Failed?, get the error reason from fbdo
            Serial.print("Error in setBool, ");
            Serial.println(fbdo.errorReason());
          }
        }
      }
    }
    else //Record is off (SwitchOn == false)
    {
      if ((x > GREENBUTTON_X) && (x < (GREENBUTTON_X + GREENBUTTON_W)))
      {
        if ((y > GREENBUTTON_Y) && (y <= (GREENBUTTON_Y + GREENBUTTON_H)))
        {
          Serial.println("Green btn hit");
          //fbdo_stream.stopWiFiClient();
          // Firebase Update
          if (Firebase.setBool(fbdo, "/SwitchOn/data", true))
          {
            //Success
            Serial.println("setBool: true success");
            greenBtn();
          }
          else
          {
            //Failed?, get the error reason from fbdo
            Serial.print("Error in setBool, ");
            Serial.println(fbdo.errorReason());
          }
        }
      }
    }

    Serial.print(x);
    Serial.print(" - ");
    Serial.print(y);
    Serial.println();

    //Stop WiFi client will gain the free memory
    //This requires more time for the SSL handshake process in the next connection
    // due to the previous connection was completely stopped.
    //fbdo.stopWiFiClient();
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
  }

  // Firebase Stream
  if (!Firebase.readStream(fbdo_stream))
  {
    Serial.println("Can't read stream data");
    Serial.println("REASON: " + fbdo_stream.errorReason());
    Serial.println();
  }

  if (fbdo_stream.streamTimeout())
  {
    Serial.println("Stream timeout, resume streaming...");
    Serial.println();
  }

  if (fbdo_stream.streamAvailable())
  {
    //Print out all information
    Serial.println("Stream Data...");
    Serial.println(fbdo_stream.streamPath());
    Serial.println(fbdo_stream.dataPath());
    Serial.println(fbdo_stream.dataType());

    //Print out the value
    //Stream data can be many types which can be determined from function dataType
    if (fbdo_stream.boolData() == 1)
    {
      Serial.println("Stream boolData: true");
      greenBtn();
    }
    else
    {
      Serial.println("Stream boolData: false");
      redBtn();
    }
  }
}
//------------------------------------------------------------------------------------------
// Global function for TFT
void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin())
  {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE))
  {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f)
      {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL)
  {
    // calibration data valid
    tft.setTouch(calData);
  }
  else
  {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL)
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f)
    {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }
}

void drawFrame()
{
  Serial.println("drawFrame called");
  tft.drawRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, TFT_BLACK);
}

// Draw a red button
void redBtn()
{
  Serial.println("redBtn called");
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_RED);
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_DARKGREY);
  drawFrame();
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ON", GREENBUTTON_X + (GREENBUTTON_W / 2), GREENBUTTON_Y + (GREENBUTTON_H / 2));
  SwitchOn = false;
  Serial.println(SwitchOn);
}

// Draw a green button
void greenBtn()
{
  Serial.println("greenBtn called");
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_GREEN);
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_DARKGREY);
  drawFrame();
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OFF", REDBUTTON_X + (REDBUTTON_W / 2) + 1, REDBUTTON_Y + (REDBUTTON_H / 2));
  SwitchOn = true;
  Serial.println(SwitchOn);
}
