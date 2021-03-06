/*
ANSI Wifi Terminal - WiFiModem ESP8266
Copyright 2015-2016 Leif Bloomquist and Alex Burger

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.
*/

/* ESP8266 port by Alex Burger */
/* Written with assistance and code from Greg Alekel and Payton Byrd */

/* ChangeLog
January 24, 2017: Leif Bloomquist
- Major refactoring plus support for ANSI WYSE Terminal instead of C64

Sept 18th, 2016: Alex Burger
- Change Hayes/Menu selection to variable
...
*/

#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>   // Special version for ESP8266, apparently
#include <EEPROM.h>
#include "Telnet.h"
#include "ADDR_EEPROM.h"

#define VERSION "ESP 0.14"

// Defines for the Adafruit Feather HUZZAH ESP8266
#define BLUE_LED 2
#define RED_LED  0

#define RX_PIN   12
#define TX_PIN   14

#define DEFAULT_BAUD_RATE 2400

unsigned int BAUD_RATE = DEFAULT_BAUD_RATE;
SoftwareSerial softSerial(RX_PIN, TX_PIN); // RX, TX

String lastHost = "";
int lastPort = TELNET_DEFAULT_PORT;

int mode_Hayes = 1;    // 0 = Menu, 1 = Hayes

void setup() 
{  
  // LEDs
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  ClearLEDs();

  // EEPROM
  EEPROM.begin(4096);
  String ssid     = readEEPROMString(ADDR_WIFI_SSID);
  String password = readEEPROMString(ADDR_WIFI_PASS);

  // Baud Rate 
  BAUD_RATE = readEEPROMInteger(ADDR_BAUD_LO);
  BAUD_RATE = ValidateBaudRate(BAUD_RATE);

  BAUD_RATE = 38400; // !!!! Testing only, for WYSE terminal

  // USB Serial connections
  Serial.begin(115200);   // Debugging
  softSerial.begin(BAUD_RATE);

  // Opening Message and connect to Wifi
  softSerial.println();
  softSerial.println();
  AnsiClearScreen(softSerial);
  softSerial.print("Connecting to ");
  softSerial.print(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    softSerial.print(".");

    attempts++;

    if (attempts > 20)
    {
        AnsiReverse(softSerial);
        softSerial.println("Connection Failed!");
        AnsiNormal(softSerial);

        ChangeSSID();  // Reboots on change
    }
  }

  softSerial.println("WiFi connected!");
  ShowInfo(true);
}


void loop()
{
    ClearLEDs();

    // Menu or Hayes AT command mode?
    mode_Hayes = EEPROM.read(ADDR_HAYES_MENU);

    if (mode_Hayes < 0 || mode_Hayes > 1)
    {
        mode_Hayes = 0;
    }

    // DEBUG !!!! Always start in Menu mode for testing.
    mode_Hayes = 0;

    if (mode_Hayes)
    {
        HayesEmulationMode();
    }
    else
    {
        ShowMenu();
    }
}

void ShowMenu()
{
    ClearLEDs();

    softSerial.print
      (F("\r\n"
         "1. Telnet to Host\r\n"
         "2. Phone Book\r\n"
         "3. Wait for Incoming Connection\r\n"
         "4. Configuration\r\n"
         "5. Hayes Emulation Mode \r\n"
         "\r\n"
         "Select: "));

  int option = ReadByte(softSerial);
  softSerial.println((char)option);   // Echo back

  switch (option)
  {
    case '1':
      DoTelnet();
      break;

    case '2':
      PhoneBook();
      break;

    case '3':
      Incoming();
      break;

    case '4':
      Configuration();
      break;

    case '5':
      EnterHayesMode();
      break;

    case '\n':
    case '\r':
    case ' ':
      break;

    default:
      softSerial.println(F("Unknown option, try again"));
      break;
    }
}

void Configuration()
{
  while (true)
  {
    char temp[30];
    softSerial.print
      (F("\r\n" 
      "Configuration Menu\r\n" 
      "\r\n"
      "1. Display Current Configuration\r\n"
      "2. Change SSID\r\n"
      "3. Change Baud Rate\r\n"
      "4. Return to Main Menu\r\n"
      "\r\nSelect: "));

    int option = ReadByte(softSerial);
    softSerial.println((char)option);  // Echo back

    switch (option)
    {
      case '1':
        ShowInfo(false);
        delay(100);        // Sometimes menu doesn't appear properly after
        break;

      case '2':
        ChangeSSID();
        break;

      case '3':
        ChangeBaudRate();
        break;

      case '4': return;

      case '\n':
      case '\r':
      case ' ':
        continue;

      default: softSerial.println(F("Unknown option, try again"));
        continue;
    }
  }
}

// ----------------------------------------------------------
// Show Configuration

void ShowInfo(boolean powerup)
{
  softSerial.println();

  softSerial.print(F("IP Address:  "));    
  softSerial.println(WiFi.localIP());

  yield();  // For 300 baud

  softSerial.print(F("IP Subnet:   "));    
  softSerial.println(WiFi.subnetMask());
   
  yield();  // For 300 baud

  softSerial.print(F("IP Gateway:  "));   
  softSerial.println(WiFi.gatewayIP());

  yield();  // For 300 baud

  softSerial.print(F("Wi-Fi SSID:  "));    
  softSerial.println(WiFi.SSID());

  yield();  // For 300 baud

  softSerial.print(F("MAC Address: "));    
  softSerial.println(WiFi.macAddress());

  yield();  // For 300 baud

  softSerial.print(F("DNS IP:      "));
  softSerial.println(WiFi.dnsIP());

  yield();  // For 300 baud

  softSerial.print(F("Hostname:    "));   
  softSerial.println(WiFi.hostname());

  yield();  // For 300 baud

  softSerial.print(F("Firmware:    "));
  softSerial.println(VERSION);

  yield();  // For 300 baud

  softSerial.print(F("Baud Rate:   "));
  softSerial.println(BAUD_RATE);

  yield();  // For 300 baud
}

void ClearLEDs()
{
    digitalWrite(BLUE_LED, HIGH);  // HIGH=Off
    digitalWrite(RED_LED, HIGH);
}

void ChangeSSID()
{
    softSerial.println();
    String input;

    softSerial.print(F("New SSID: "));
    input = GetInput();   

    if (input.length() == 0) return;

    updateEEPROMString(ADDR_WIFI_SSID, input);

    softSerial.println();

    softSerial.print(F("Passphrase: "));
    input = GetInput();
    updateEEPROMString(ADDR_WIFI_PASS, input);

    softSerial.println(F("\r\nSSID changed.  Rebooting..."));
    delay(1000);
    ESP.restart();
    while (1);
}


void TerminalMode(WiFiClient client, WiFiServer &server)
{
  int i = 0;
  char buffer[10];
  int buffer_index = 0;
  int buffer_bytes = 0;
  bool isFirstChar = true;
  bool isTelnet = false;
  bool telnetBinaryMode = false;

  while (client.connected())
  {
      // 0. Let ESP8266 do its stuff in the background
      yield();

      // 1. Reset LEDs
      ClearLEDs();

      // 2. Get data from the telnet client and push it to the serial port
      if (client.available() > 0)
      {
          digitalWrite(BLUE_LED, LOW);  // Low=On

          char data = client.read();

          // If first character back from remote side is NVT_IAC, we have a telnet connection.
          if (isFirstChar)
          {
              if (data == NVT_IAC)
              {
                  isTelnet = true;
                  CheckTelnet(isFirstChar, telnetBinaryMode, client);
              }
              else
              {
                  isTelnet = false;
              }
              isFirstChar = false;
          }
          else  // Connection already established, but may be more telnet control characters
          {
              if ((data == NVT_IAC) && isTelnet)
              {
                  if (CheckTelnet(isFirstChar, telnetBinaryMode, client))
                  {
                      softSerial.write(NVT_IAC);
                  }
              }
              else   //  Finally regular data - just pass the data along.
              {
                  softSerial.write(data);
                  delayMicroseconds(200);  // 200 seems to be the limit to keep WYSE term from losing characters
              }
          }
      }

      // 3. Check serial port for data and push it to the telnet client
      if (softSerial.available())
      {
          digitalWrite(RED_LED, LOW);  // Low=On

          char data = softSerial.read();
          client.write((char)data);  // Weird!
          delay(1);

          // 3a. Check Escape (+++).  Just exit immediately if found (which disconnects)
          if (CheckEscape(data))
          {
              AnsiReverse(softSerial);
              softSerial.println("Escape!");
              AnsiNormal(softSerial);
              return;
          }
      }

     // 4. Check for new incoming connections - reject
     if (server.hasClient())
     {
        WiFiClient newClient = server.available();
        RejectIncoming(newClient);
    }

  } // while (client.connected())
}

// EOF