#include <Arduino.h>
#include "Wire.h"
#include "M5UNIT_DIGI_CLOCK.h"
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Time.h>
/*****************************************************
 * Code to use the M5 Stack Digiclock Unit as a digital clock set be getting the time from
 * an NTP server via WiFi and using the ESP32 build in real time clock.
 * References:
 *  NTPClient:        https://github.com/arduino-libraries/NTPClient/tree/master
 *  ESP32Time:        https://github.com/fbiego/ESP32Time/tree/main
 *  Digi-clock Unit:  https://docs.m5stack.com/en/unit/digi_clock
 * 
 */
//Put your WiFi credential here or add code to read them from a file which could be encrypted etc.
const char *ssid     = "Your Wifi SSID";
const char *password = "Your Wifi Password";
//Enable NTP and Real-time clock access
WiFiUDP ntpUDP;
ESP32Time rtc(-25200); //(3600 * 7) * -1);  // offK4tj3N34k3n!et in seconds GMT-7 (MST); 1 hour is 3600 seconds.

// You can specify the time server pool and the offset, (in seconds)
// additionally you can specify the update interval (in milliseconds).
// NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
// By default 'pool.ntp.org' is used with 60 seconds update interval and no offset
NTPClient ntp(ntpUDP);

/* For ESP32-WROOM-32U board */
#define SDA 21
#define SCL 22
#define ADD 0x30 //I2C address of Digi-Clock unit (can be changed via dip switch)
#define FLASHPIN 32 // Used to read dip switch 
#define HOUR24PIN 33

M5UNIT_DIGI_CLOCK clk;
bool flashColon = false; //If false will not flash the colon and the showColon value is ignored
bool showColon = true;   //Used if flashColon is true to make colon flash once per second
bool use24HourMode = false; //Set to true if you want to display hours like 13:00 for 1:00 PM etc.


/// @brief Get the time and return it as a String for display
/// @param includeColon If true the colon will be displayed between hour and minute
/// @return Returns the time as "HH:MM" in 12 or 24 hour format
String getTimeStr(bool includeColon = true)
{
  // GetTime uses the standard c++ strftime() parameters: https://cplusplus.com/reference/ctime/strftime/
  // Use %I for 12 Hour Time hours or %H for 24 Hour time.  i.e 1 PM is 13 hours in 24 Hour time format.
  String ts;
  if (use24HourMode)
  {
    //Display the hour and minutes in 24 Hour format
    if (includeColon)
      ts =  rtc.getTime("%H:%M");
    else
      ts = rtc.getTime("%H%M");
  }
  else
  {
    if (includeColon)
      ts = rtc.getTime("%I:%M");
    else
      ts =  rtc.getTime("%I%M");
  }
  if (ts[0] == '0') ts[0] = ' ';
  return ts;
}

/// @brief Read the mode switch and set global variables accordingly.
void readModeSwitch()
{
  flashColon = (digitalRead(FLASHPIN) == LOW);
  use24HourMode = (digitalRead(HOUR24PIN) == LOW);
}

/// @brief Setup and initialize the various objects and resources needed by the application
void setup() 
{
  pinMode(FLASHPIN, INPUT_PULLUP);
  pinMode(HOUR24PIN, INPUT_PULLUP);
  readModeSwitch();
  /* Digital clock init */
  if (!clk.begin(&Wire, SDA, SCL, ADD)) 
  {
    // Darn-it the digi-clock unit won't start; check I2C address and connections...
    Serial.begin(115200);  //Can't use the clock display so send a error to the serial monitor
    delay(500);
    Serial.println("Digital Clock Error!");
    while (true); //Make it stop everything
  } 
  char bufr[] = "    ";
  clk.setString(bufr); //Blank out the clock display
  clk.setBrightness(6);
  WiFi.begin(ssid, password); //Try to connect to WiFi
  int count = 0;
  while ( WiFi.status() != WL_CONNECTED )
  {
    count++;
    delay(500); //Retry every half second
    sprintf(bufr, "%d", count);
    clk.setString(bufr); //Show how many half seconds we had to wait for WiFi to respond
    if (count > 30) //If no connection after 15 seconds assume there is a problem and halt
    {
      //WiFi did not respond in time, check if the Router is accessible and SID and Password are correct
      String err = "Err"; //Display Err on the clock display
      err.toCharArray(bufr, 5);
      clk.setString(bufr);
      while (true);
    }
    //Serial.print ( "." ); //Decided not to sent stuff to the Serial monitor unless we can't use the clock display
  }
  ntp.begin();  // Initialize the NTP service via UDP WiFI connection
  //ntp.setTimeOffset(-7); does not seem to work!  We will do it in the ESP32 real time clock instead...
  Wire.begin(SDA, SCL);
  ntp.update();
  //Alright we have the exact time so now we can set the real time clock
  rtc.setTime(ntp.getSeconds(), ntp.getMinutes(), ntp.getHours(), ntp.getDay(), 1, 2023); // sec, min, hr, day, month, year 17th Jan 2021 15:24:30
  String ts = getTimeStr();
  ts.toCharArray(bufr, 5);
  clk.setString(bufr);
  ntp.setUpdateInterval(3600000L);  //Tell NTP not to worry about updating more than once an hour since we are using the real time clock
                                    // and it is accurate enough to stay in tune for 60 minutes.
}

int startHour = 0;

void loop() 
{
  //readModeSwitch();
  int hourNow = rtc.getHour(true);
  if (startHour != hourNow); //About once an hour update the real-time clock
  {
    startHour = hourNow;
    ntp.update();
    //Update the real time clock incase it has drifted
    rtc.setTime(ntp.getSeconds(), ntp.getMinutes(), ntp.getHours(), ntp.getDay(), 1, 2023); // sec, min, hr, day, month, year 17th Jan 2021 15:24:30
  }
  String ts;
  char buf[8];
  if (flashColon)
  {
    ts = getTimeStr(showColon);
    showColon = !showColon;
  }
  else
  {
    ts = getTimeStr(true);
  }
  ts.toCharArray(buf, 8);
  clk.setString(buf); //Update clock display
  delay(1000);
}

/* Unused code
    // char buff[] = "8.8.:8.8.";
    // Digiclock.setString(buff);

    // for (int i = 0; i < 5; i++) 
    // {
    //     Digiclock.setBrightness(9);
    //     delay(333);
    //     Digiclock.setBrightness(0);
    //     delay(333);
    // }
    // delay(100);

    // for (int i = 0; i < 4; i++) 
    // {
    //     for (uint8_t i = 0; i < 9; i++) 
    //     {
    //         Digiclock.setBrightness(i);
    //         delay(20);
    //     }
    //     for (uint8_t i = 8; i > 0; i--) 
    //     {
    //         Digiclock.setBrightness(i);
    //         delay(20);
    //     }
    // }
    // delay(100);

    //Digiclock.setBrightness(9);
    // for (int j = 0; j < 3; j++) 
    // {
    //     for (int i = 0; i < 10; i++) 
    //     {
    //         sprintf(buff, "%d.%d.:%d.%d.", i, i, i, i);
    //         Digiclock.setString(buff);
    //         Serial.println(buff);
    //         delay(200);
    //     }
    // }
    // delay(100);

    // Digiclock.setBrightness(9);
    // // for (;;) 
    // // {
    //     char buff2[] =  "12:00";
    //     Digiclock.setString(buff2);
    //     Serial.printf(buff2);
    //     delay(1000);
    //     char buff3[] =  "1200";
    //     Digiclock.setString(buff3);
    //     Serial.printf(buff3);
    //     delay(1000);
    // // }
//  tc.update();
*/