#include "sha1.h"
#include "mysql.h"
#include <SPI.h>
#include "Wire.h"
#define DS3231_I2C_ADDRESS 0x68
#include <Ethernet.h>
#include <ICMPPing.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Dewpnt_heatIndx.h"
#include "util.h"
#include <dht.h>
dht DHT;

//  ping server socket
SOCKET pingSocket = 0;
char buffer [256];
ICMPPing ping(pingSocket, (uint16_t)random(0, 255));


const char htmlStyleMultiline[] PROGMEM = "<style>"
    ".tooltip {"
    "    position: relative;"
    "    display: inline-block;"
    "   border-bottom: 1px dotted black;"
    "}"

    ".tooltip .tooltiptext {"
    "   visibility: hidden;"
    "   width: 120px;"
    "   background-color: black;"
    "   color: #fff;"
    "   text-align: center;"
    "    border-radius: 6px;"
    "    padding: 5px 0;"

    "    /* Position the tooltip */"
    "    position: absolute;"
    "    z-index: 1;"
    "}"

    ".tooltip:hover .tooltiptext {"
    "    visibility: visible;"
    "}"
    "</style>";

bool connected = false;
bool wateringBasedOnAlarm = true;

// Define the number of samples to keep track of.  The higher the number,
// the more the readings will be smoothed, but the slower the output will
// respond to the input.  Using a constant rather than a normal variable lets
// use this value to determine the size of the readings array.
const int numReadings = 30;

// if val of soil moisture is greater than 30 turn on solenoid
const int8_t DRY_SOIL_DEFAULT = 30;

int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

/* Setup for the Connector/Arduino */
Connector my_conn; // The Connector/Arduino reference

char user[] = "arduino";
char password[] = "arduino";
String INSERT_SQL = "";

bool power_to_solenoid = false;

#define ONE_WIRE_BUS 42 /*-(Connect to Pin 42 )- brown wire out on potentiometer*/
/* Set up a oneWire instance to communicate with any OneWire device*/
OneWire ourWire(ONE_WIRE_BUS);

/* Tell Dallas Temperature Library to use oneWire Library */
DallasTemperature tempSensor(&ourWire);

int mint = 0;

// size of buffer used to capture HTTP requests
#define REQ_BUF_SZ   50

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 156);
IPAddress server_addr(192, 168, 1, 3);
EthernetClient client;

String data;
int result;
String readString;
// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 8025 is default for HTTP):
EthernetServer server(8025);

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer


String rainMsg;
unsigned long sqlInsertInterval = 300000; // the repeat interval after 5 minutes

unsigned long timer = 0; // timer for sql insert after 5 minutes
unsigned long previousOnBoardLedMillis = 0;   // will store last time the LED was updated
const int blinkDuration = 5000; // number of millisecs that Led's are on - all three leds use this
const int onBoardLedInterval = 5000; // number of millisecs between blinks

// ///////////// PIN Setup /////////////////
const int buttonPin = 3;
int flag = 0;
int buttonState = 0;
const int solenoidPin = 6;    // D6 : This is the output pin on the Arduino we are using

#define DHT11_PIN 8 // D8 to 2nd pin on DHT, leave 3rd pin unconnected 4th is gnd
const int8_t rainsense = 0; // analog sensor input pin A0
const int buzzerout = 4; // digital output pin D2 - buzzer output
const int8_t pwatLvlSense = A2; // Pot Water Level Sensor A2 connected with blue wire
int SENSE = 1; // Soil Sensor input at Analog PIN A1 Green wire
int val = 0;

int lightSensorPin = 3; // A3, earlier it was A8
int analogValue = 0;

double tempInC;
double indorTempinC;
double indorTempInF;
int humidity;
float dP; // dew point
float dPF; // dew point in fahrenheit
float tF; // temperature in fahrenheit

double hIinFah;
double hIinCel;

String soilMsg;

// the interval in mS
unsigned long interval = 10000; // the time we need to wait to finish watering the plant.
unsigned long currentMillis = 0;    // stores the value of millis() in each iteration of loop()
unsigned long previousMillis = 0; // millis() returns an unsigned long.

void setup() {
  Wire.begin();
  power_to_solenoid = false;
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(solenoidPin, OUTPUT); // Sets the pin as an output
  pinMode(buzzerout, OUTPUT);
  pinMode(rainsense, INPUT);

  // initialize all the readings to 0:
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  tempSensor.begin();
  startEthernet();

  // connected = my_conn.mysql_connect(server_addr, 3306, user, password);

  server.begin();
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB port only
  }

  timer = millis(); // start timer

}

void startEthernet()
{
  client.stop();
  Ethernet.begin(mac, ip);
  // give the Ethernet shield a second to initialize:
  delay(10);
  // Serial.print(F("connected. My IP is "));
  // Serial.println(Ethernet.localIP());
}

bool waterThePlant()
{

  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  util::readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                       &year);

  if ((hour  == 16 || hour == 8) and (minute == 10) and (second <= 10) ) {
    digitalWrite(solenoidPin, HIGH);
    digitalWrite(buzzerout, HIGH);
    power_to_solenoid = true;
    return power_to_solenoid;
  }
  // turn of solenoid after 10 seconds set by interval variable.
  else if (wateringBasedOnAlarm == false)
  {
    digitalWrite(solenoidPin, LOW);
    digitalWrite(buzzerout, LOW);
    power_to_solenoid = false;
    return power_to_solenoid;

  } else

  {
    // set to false which is also the default state set in beginning.
    digitalWrite(solenoidPin, LOW);
    power_to_solenoid = false;
    return power_to_solenoid;
  }

}

// send the XML file containing analog value
void outputJson(EthernetClient client)
{
  client.print("{\"arduino\":[{\"location\":\"outdoor\",\"celsius\":\"");
  client.print(tempInC);
  client.print("\"}]}");
  client.println();
}

void loop()
{
  buttonState = digitalRead(buttonPin);
  currentMillis = millis();   // capture the latest value of millis()

  int watrLvlSnsr = analogRead(pwatLvlSense);

  // subtract the last reading:
  total = total - readings[readIndex];
  // read from the sensor:
  val = analogRead(SENSE);
  readings[readIndex] =  val / 10;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  val = total / numReadings;

  int rainSenseReading = analogRead(rainsense);

  //  tempInC = util::getTempHumdata(DHT11_PIN)[0];
  //  humidity = util::getTempHumdata(DHT11_PIN)[1];
  delay(200); // 2 seconds stop for DHT to read
  //  int chk = DHT.read11(DHT11_PIN);
  tempSensor.requestTemperatures(); // Send the command to get temperatures
  tempInC = tempSensor.getTempCByIndex(0);
  tF = tempSensor.getTempFByIndex(0);
  //  humidity = DHT.humidity;
  //  indorTempinC = DHT.temperature;
  //  indorTempInF = (indorTempinC * 9.0) / 5.0 + 32;
  //  dP = (Dewpnt_heatIndx::dewPointFast(indorTempinC, humidity));
  //  dPF = ((dP * 9) / 5) + 32;

  analogValue = analogRead(lightSensorPin);
  // Serial.print(F("LDR value: "));
  // Serial.print(analogValue);
  //  digitalWrite (redLedPin, analogValue < 55) ;
  //  digitalWrite (yellowLedPin, analogValue >= 55 && analogValue <= 100) ;
  //  digitalWrite (greenLedPin, analogValue > 100) ;
  // Serial.print(F(", Soil Moisture: "));
  // Serial.println(val);

  wateringBasedOnAlarm = waterThePlant();

  // if not time to water plant check soil moisture
  if (!wateringBasedOnAlarm) {
    if (val < DRY_SOIL_DEFAULT ) {
      power_to_solenoid = true;
      soilMsg = F("Soil is dry.");
    } else soilMsg = F("Soil is damp.");


    if (rainSenseReading < 500)  {
      rainMsg = F("It is raining heavily !");
      power_to_solenoid = false;
    }
    if (rainSenseReading < 300) {
      rainMsg = F("Moderate rain.");
      power_to_solenoid = false;
      soilMsg = F("Watering plant is turned off when it is raining.");
    }
    if (rainSenseReading < 200) {
      rainMsg = F("Light Rain Showers !");
      power_to_solenoid = false;
      soilMsg = F("Watering plant is turned off when it is raining.");
    }
    if (rainSenseReading > 500) {
      rainMsg = F("Not Raining.");
      power_to_solenoid = true;
    }
    else power_to_solenoid = false;
    int hr = util::getHour();
    // Do not water plant at night
    if ((hr >= 19  && hr <= 23) || (hr >= 0 && hr <= 7)) {
      power_to_solenoid = false;
      soilMsg = ". Watering plant is turned off at night.";
    }
    if (watrLvlSnsr >= 400) {
      power_to_solenoid = false;
    }
    // Turn on Solenoid Valve if soil moisture value less than 25
    if (power_to_solenoid == true)
    { // i.e. if onBoardLedState is HIGH
      soilMsg = F("Soil is dry. ");
      // do not water pot if its raining or if it is night.
      if (analogValue > 300) {
        soilMsg = soilMsg +  F("Watering the plant.");
        // turn solenoid off after 45 sec
        digitalWrite(solenoidPin, HIGH);
        digitalWrite(buzzerout, HIGH);
      }
      // if the Led is on, we must wait for the duration to expire before turning it off
      if (currentMillis - previousOnBoardLedMillis >= blinkDuration) {
        // time is up, so change the state to LOW
        power_to_solenoid = false; // don't execute this again
        digitalWrite(solenoidPin, LOW);
        digitalWrite(buzzerout, LOW);
      }
      // and save the time when we made the change
      previousOnBoardLedMillis += blinkDuration;
    } else
    {
      soilMsg = F("Soil is damp. ");
      power_to_solenoid = false; // don't execute this again
      digitalWrite(solenoidPin, LOW);
      digitalWrite(buzzerout, LOW);
    }
  }
  //If button pressed...
  if (buttonState == LOW) {
    //...ones, turn led on!
    if ( flag == 0) {
      digitalWrite(solenoidPin, HIGH);
      digitalWrite(buzzerout, HIGH);
      flag = 1; //change flag variable
    }
    //...twice, turn led off!
    else if ( flag == 1) {
      digitalWrite(solenoidPin, LOW);
      digitalWrite(buzzerout, LOW);
      flag = 0; //change flag variable again
    }
  }
  EthernetClient client = server.available();  // try to get client

  if (client) {  // got client?
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {   // client data available to read
        char c = client.read(); // read 1 byte (character) from client
        // buffer first part of HTTP request in HTTP_req array (string)
        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
        if (req_index < (REQ_BUF_SZ - 1)) {
          HTTP_req[req_index] = c;          // save HTTP request character
          req_index++;
        }

        //read char by char HTTP request
        if (readString.length() < 100) {
          //store characters to string
          readString += c;
        }

        // last line of client request is blank and ends with \n
        // respond to client only after last line received
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          // remainder of header follows below, depending on if
          // web page or XML page is requested
          // Ajax request - send XML file
          if (util::StrContains(HTTP_req, "json")) {
            // send rest of HTTP header
            // client.println("Content-Type: application/json;charset=utf-8");
            client.println(F("Content-Type: text/html"));
            client.println("Access-Control-Allow-Origin: *");
            client.println("Keep-Alive: timeout=2, max=100");
            client.println("Connection: keep-alive");
            client.println();
            outputJson(client);
          } else if (util::StrContains(HTTP_req, "/?waterPlant1")) {
            client.println(F("Content-Type: text/html"));
            client.println("Access-Control-Allow-Origin: *");
            client.println(F("Connection: close"));
            client.println();
            client.println(F("<!DOCTYPE HTML>"));
            client.println(F("<html><head><title>"));
            client.println(F("Welcome to Arduino WebServer</title>"));
            client.println("<meta http-equiv='refresh' content='0; url=../'>");
            client.println(F("</head><body></body></html>"));
          }
          else {  // web page request
            // send rest of HTTP header
            client.println(F("Content-Type: text/html"));
            client.println(F("Connection: close"));  // the connection will be closed after completion of the response
            client.println(F("Refresh: 1000"));  // refresh the page automatically every 60 sec
            client.println();
            //            sentHeader = true;


            client.println(F("<!DOCTYPE HTML>"));
            client.println(F("<html><head><title>"));
            client.println(F("Welcome to Arduino WebServer</title>"));
            client.println("<meta name='apple-mobile-web-app-capable' content='yes' />");
            client.println("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />");
            client.println("<!--link rel='stylesheet' type='text/css' href='http://randomnerdtutorials.com/ethernetcss.css' /-->");
            util::printProgStr(client, htmlStyleMultiline );
            client.println(F("</head><body style='background-color:grey'>"));
            client.println(c);
            client.print(F("<p style='color:red';style='font-family: Arial'> LIVE: </p>"));
            util::displayTime(client);
            if (rainSenseReading < 500) {
              client.print(rainMsg);
              client.print(F(" Rain Sensor reads: "));
              client.println(rainSenseReading);
            }

            client.print(F("<br>Outdoor Temperature: <u>+</u>"));
            client.print(tempInC, 1);
            client.print(F("&#8451; / "));
            client.print(tF);
            client.println(F(" &#8457;"));
            //          client.print("<br>Indoor temperature: ");
            //          client.print(indorTempinC);
            //          client.print("&#8451;/ ");
            //          client.println(indorTempInF);
            //          client.println(F(" &#8457;, Humidity: "));
            //
            //          client.print(humidity, 1);
            //          client.print(F("%<br>Dew Point: "));
            //          client.print(dP);
            //          client.print(F("&#8451; Heat Index: "));
            //
            //          if (tF < 70 || humidity < 30) {
            //            client.print(tempInC);
            //            client.print(F(" &#8451; / "));
            //            client.print(tF);
            //            client.println(F(" &#8457;"));
            //          }
            //          else {
            //            hIinFah = Dewpnt_heatIndx::heatIndex(tF, humidity);
            //            hIinCel = (hIinFah + 40) / 1.8 - 40;
            //            client.print(hIinCel);
            //            client.print(F(" &#8451;/ "));
            //            client.print(hIinFah);
            //            client.println(F(" &#8457; <br>"));
            //          }

            if (analogValue < 10) {
              client.println(F("<br><div class='tooltip'>It is  <font style='color:red';>dark</font>."));
            }
            else if (analogValue > 10 && analogValue < 50) {
              client.println(F("<br><div class='tooltip'>Fair amount of<font style='color:yellow';><b>light</b></font>, but not very bright"));
            }
            else if (analogValue >= 50 && analogValue <= 100) {
              client.println(F("<br><div class='tooltip'>Fair amount of <font style='color:yellow';><b>light</b></font>, but not bright enough."));
            }
            else {
              client.println(F("<br><div class='tooltip'>It is Full Bright Day<font style='color:green';><b> :) </b></font>."));
            }

            client.print(F("<span class='tooltiptext'> LDR Value Reads: "));
            client.print(analogValue);
            client.println(F("</span></div>"));
            client.print(F("<br>Pot Soil Moisture: "));
            client.print(val);
            client.print(F(" <a href=\"/?waterPlant1\"\">Water the plant in pot.</a>"));
            client.println(soilMsg);
            if (wateringBasedOnAlarm) {
              client.println(F("Watering plant based on set alarm ."));
            }
            client.println (F("</body></html>"));
          }
          // display received HTTP request on serial port
          //Serial.print(HTTP_req);
          // reset buffer index and all buffer elements to 0
          req_index = 0;
          util::StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
        // every line of text received from the client ends with \r\n
        if (c == '\n') {
          // last character on line of received text
          // starting new line with next character read
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // a text character was received from client
          currentLineIsBlank = false;
        }
      } // end if (client.available())
    } // end while (client.connected())
    delay(1);      // give the web browser time to receive the data
    client.stop(); // close the connection

  } // end if (client)

  //controls the Arduino if you press the buttons
  if (readString.indexOf("/?waterPlant1") > 0) {
    // check if "interval" time has passed (1000 milliseconds)
    digitalWrite(solenoidPin, HIGH); // sets the solenoid power based on power_to_solenoid
  }

  if ((unsigned long)(currentMillis - previousMillis) >= interval) {
    //clearing string for next read
    readString = "";
    wateringBasedOnAlarm = false;
    flag = 0; //change flag variable again
    // save the "current" time
    previousMillis = millis();
  }

  // perform sql insert after 5 minutes
  //  if ((millis() - timer) > sqlInsertInterval) {
  //    ICMPEchoReply echoReply = ping(server_addr, 4);
  //    if (echoReply.status == SUCCESS)
  //    {
  //      // timed out
  //      timer += sqlInsertInterval;// reset timer by moving it along to the next interval
  //
  //      // inserting to sql database on mysql server
  //      INSERT_SQL = "";
  //      INSERT_SQL.concat("INSERT INTO arduinoSensorData.outTempLog (out_temperature) VALUES ('");
  //      INSERT_SQL.concat(tempInC);
  //      INSERT_SQL.concat("');");
  //
  //      const char *mycharp = INSERT_SQL.c_str();
  //      if (!connected) {
  //        my_conn.mysql_connect(server_addr, 3306, user, password);
  //        connected = true;
  //
  //      }
  //      else if (connected == true)
  //      {
  //        Serial.print("Inserting : ");
  //        Serial.println(INSERT_SQL);
  //        Serial.println("Connection Successfull,inserting to database.");
  //
  //        my_conn.cmd_query(mycharp);
  //
  //      }
  //      else {
  //        Serial.println("Connection failed.");
  //      }
  //    }
  //  }


}

