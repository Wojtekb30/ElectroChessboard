#include <Wire.h>
#include <RTClib.h>
#include <SoftwareSerial.h>

// Initialize SoftwareSerial: RX on Pin 10, TX on Pin 19 (PD2)
SoftwareSerial MyFix(10, 19); 
RTC_DS1307 rtc;

void setup() {
  // Start the SoftwareSerial port at 9600 baud
  MyFix.begin(9600);
  
  // Initialize I2C for the RTC
  if (!rtc.begin()) {
    MyFix.println("RTC not found!");
    while (1);
  }

  // Ensure the clock is running
  if (!rtc.isrunning()) {
    MyFix.println("RTC was stopped, setting time now...");
    // Sets the RTC to the time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  DateTime now = rtc.now();

  // Construct the time string
  MyFix.print(now.year(), DEC);
  MyFix.print('/');
  MyFix.print(now.month(), DEC);
  MyFix.print('/');
  MyFix.print(now.day(), DEC);
  MyFix.print(" - ");
  MyFix.print(now.hour(), DEC);
  MyFix.print(':');
  if (now.minute() < 10) MyFix.print('0'); // Leading zero
  MyFix.print(now.minute(), DEC);
  MyFix.print(':');
  if (now.second() < 10) MyFix.print('0'); // Leading zero
  MyFix.println(now.second(), DEC);
  MyFix.println(now.timestamp());

  delay(1000);
}