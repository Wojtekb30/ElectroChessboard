/*
  SD card write/read test, then list files using SD.open("/")
  Board: Arduino Mega (CS = 53)
*/

#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>

#define TEXTTOWRITE "This is text that will be written to the file."

// Software serial for logging (adjust pins if needed)
SoftwareSerial MyFix(10, 19); // RX, TX

const int chipSelect = 53;
File myFile;

void listFiles() {
  MyFix.println("\nFiles on card:");

  File root = SD.open("/");
  if (!root) {
    MyFix.println("Failed to open root directory");
    return;
  }

  File entry = root.openNextFile();
  if (!entry) {
    MyFix.println(" <no files found>");
  }
  while (entry) {
    if (entry.isDirectory()) {
      MyFix.print("DIR  : ");
      MyFix.print(entry.name());
      MyFix.println();
    } else {
      MyFix.print("FILE : ");
      MyFix.print(entry.name());
      MyFix.print("\t");
      MyFix.print(entry.size());
      MyFix.println(" bytes");
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
}

void setup() {
  MyFix.begin(9600);
  
  MyFix.println("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    MyFix.println("SD init failed! Check wiring and card.");
    while (true) {
      // stop here so you can debug
    }
  }
  MyFix.println("SD init OK");

  // Overwrite the file each boot so you don't accumulate lines
  SD.remove("test.txt");

  MyFix.print("Writing test.txt... ");
  myFile = SD.open("test.txt", FILE_WRITE);
  if (myFile) {
    myFile.println(TEXTTOWRITE);
    myFile.close();
    MyFix.println("done");
  } else {
    MyFix.println("FAILED to open test.txt for writing");
  }

  // Read it back once
  MyFix.println("\nReading test.txt:");
  myFile = SD.open("test.txt");
  if (myFile) {
    while (myFile.available()) {
      MyFix.write(myFile.read());
    }
    myFile.close();
    MyFix.println(); // newline after file contents
  } else {
    MyFix.println("Error opening test.txt for reading");
  }

  // List files in root directory (this will show test.txt)
  listFiles();

  MyFix.println("\nDone.");
}

void loop() {
  // nothing here
}
