#include <RunningAverage.h>
#include "sha1.h"
#include "mysql.h"
#include <SPI.h>
#include "Wire.h"
#include "StringStream.h"
#define DS3231_I2C_ADDRESS 0x68
#include <Ethernet.h>
//#include <ICMPPing.h>

#include "Dewpnt_heatIndx.h"
#include "util.h"
#include "DHT.h"



#define DRNGROMDHTPIN 4
#define OUTDOORDHTPIN 14
#define DHTTYPE DHT22   // DHT 22  (AM2302)

// Initialize DHT sensor for normal 16mhz Arduino
DHT dhtDrwRom(DRNGROMDHTPIN, DHTTYPE);
DHT dhtOutdoor(OUTDOORDHTPIN, DHTTYPE);

//  ping server socket
//SOCKET pingSocket = 0;
//char buffer [256];
//ICMPPing ping(pingSocket, (uint16_t)random(0, 255));

bool wateringBasedOnAlarm = true;

// Define the number of samples to keep track of.  The higher the number,
// the more the readings will be smoothed, but the slower the output will
// respond to the input.  Using a constant rather than a normal variable lets
// use this value to determine the size of the readings array.
const int numReadings = 20;

// if val of soil moisture is greater than 75 turn on solenoid
const int8_t DRY_SOIL_DEFAULT = 75;
RunningAverage myRA(numReadings);

/* Setup for the Connector/Arduino */
Connector my_conn; // The Connector/Arduino reference
bool connected = false;

const char user[] = "arduino";
const char password[] = "arduino";
String insertSql = "";
//char insertSql[295];

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
// Initialize the Ethernet server library with the IP address
// and port you want to use (port 8025 is default for HTTP):
EthernetServer server(8025);

char HTTP_req[REQ_BUF_SZ] = {0}; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer

String rainMsg;
unsigned long sqlInsertInterval = 10;

unsigned long timer = 0; // timer for sql insert after 5 minutes
unsigned long previousOnBoardLedMillis = 0;   // will store last time the LED was updated
const int blinkDuration = 5000; // number of millisecs that Led's are on - all three leds use this
const int onBoardLedInterval = 5000; // number of millisecs between blinks

// ///////////// PIN Setup /////////////////
const int buttonPin = 3;
int flag = 0;
int buttonState = 0;
const int solenoidPin = 6;    // D6 : This is the output pin on the Arduino we are using
bool soilSensorState = LOW;
#define power_soilMoist_pin 8 // D8 to 2nd pin on DHT, leave 3rd pin unconnected 4th is gnd
const int8_t rainsense = A0; // analog sensor input pin A0

const int8_t pwatLvlSense = A2; // Pot Water Level Sensor A2 connected with blue wire
int SENSE = A1; // Soil Sensor input at Analog PIN A1 Green wire
bool power_to_solenoid = false;

int val = 0;
int avgVal = 0;

int lightSensorPin = 3; // A3, earlier it was A8
int analogValue = 0;

String soilMsg = "No update";

// the interval in mS
unsigned long interval = 10000; // the time we need to wait to finish watering the plant.
unsigned long currentMillis = 0;    // stores the value of millis() in each iteration of loop()
unsigned long previousMillis = 0; // millis() returns an unsigned long.


void setup() {
  myRA.clear();
  // begin Realtime clock module
  Wire.begin();
  // to set the initial time here:
  // DS3231 seconds, minutes, hours, day, date, month, year
  //  util::setDS3231time(23, 20, 23, 5, 12, 1, 17); // India Time is set
  dhtDrwRom.begin();
  dhtOutdoor.begin();
  power_to_solenoid = false;
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(SENSE, INPUT);
  pinMode(power_to_solenoid, OUTPUT);
  digitalWrite(power_to_solenoid, LOW);

  pinMode(solenoidPin, OUTPUT); // Sets the pin as an output

  pinMode(rainsense, INPUT);

  power_to_solenoid = false;
  startEthernet();
  // connect to mysql server
  connected = my_conn.mysql_connect(server_addr, 3306, user, password);

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
  Serial.print(F("connected. My IP is "));
  Serial.println(Ethernet.localIP());
}

bool waterThePlant()
{

  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  util::readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                       &year);

  if ((hour  == 16 || hour == 8) and (minute == 10) and (second <= 10) ) {
    digitalWrite(solenoidPin, HIGH);
    power_to_solenoid = true;
    return power_to_solenoid;
  }
  // turn of solenoid after 10 seconds set by interval variable.
  else if (wateringBasedOnAlarm == false)
  {
    digitalWrite(solenoidPin, LOW);
    power_to_solenoid = false;
    return power_to_solenoid;

  } else

  {
    digitalWrite(solenoidPin, LOW);
    // explicitly set to false which is also the default state set in beginning.
    power_to_solenoid = false;
    return power_to_solenoid;
  }

}

float outdoorTempInC;
float outdoorTempInF;
float outoorHumidity;
float outdoordP;
float outdoordPF;
float outdoorhi;
float outdoorhIinFah;
float outdoorhIinCel;

float indorTempinC;
float indorTempinF;
float indoorHumidity;
float dP; // dew point
float dPF; // dew point in fahrenheit
float tF; // temperature in fahrenheit
float hi; // heat index in fahrenheit of indoor
float hIinFah;
float hIinCel;  // heat index in celcius of indoor

int soilMoistCounter = 0;

// send the JSON containing analog value
void outputJson(EthernetClient client, bool formatted = false)
{
  client.print("{\"arduino\" : [{\"location\" : \"Outdoor\" , \"temperatureInC\" : \"");
  client.print(outdoorTempInC);
  client.print("\" , \"temperatureInF\" : \"");
  client.print(outdoorTempInF);
  client.print("\" , \"dewPoint_in_Fahr\" : \"");
  client.print(outdoordPF);
  client.print("\" , \"dewPoint_in_Cel\" : \"");
  client.print(outdoordP);
  client.print("\" , \"heat_index__in_Fahr\" : \"");
  client.print(outdoorhIinFah);
  client.print("\" , \"heat_index_in_Cel\" : \"");
  client.print(outdoorhIinCel);
  client.print("\" , \"humidity\":\"");
  client.print(outoorHumidity);

  client.print("\"} , {\"location\" : \"Drawing Room\" , \"temperatureInC\" : \"");
  client.print(indorTempinC);
  //  client.print("\" , \"localTime\" : \"");
  //  client.print(
  client.print("\" , \"temperatureInF\" : \"");
  client.print(indorTempinF);
  client.print("\" , \"dewPoint_in_Fahr\" : \"");
  client.print(dPF);
  client.print("\" , \"dewPoint_in_Cel\" : \"");
  client.print(dP);
  client.print("\" , \"heat_index__in_Fahr\" : \"");
  client.print(hi);
  client.print("\" , \"heat_index_in_Cel\" : \"");
  client.print(hIinCel);
  client.print("\" , \"humidity\" : \"");
  client.print(indoorHumidity);
  client.print("\"}] , \"pots\" : [{\"soilMoist\" : \"");
  client.print(val);
  client.print("\", \"avgSoilMoist\" :\"");
  client.print(avgVal);
  client.print("\" }]}");
  client.println();
}


String getSqlInsertString()
{
  // inserting to sql database on mysql server
  insertSql = "";
  insertSql.concat("INSERT INTO arduinoSensorData.sensorLog (out_temperature, out_humidity, ");
  insertSql.concat(" drwngRoom_temperature, drwngRoom_humidity, pot1_soilMoisture, pot1_avg_SoilMoisture, ");
  insertSql.concat("wateringPot1) VALUES ('");
  insertSql.concat(outdoorTempInC);
  insertSql.concat("', '");
  insertSql.concat(outoorHumidity);
  insertSql.concat("', '");
  insertSql.concat(indorTempinC);
  insertSql.concat("', '");
  insertSql.concat(indoorHumidity);
  insertSql.concat("', '");
  insertSql.concat(val);
  insertSql.concat("', '");
  insertSql.concat(avgVal);
  insertSql.concat("', 'N/A");
  //      if (wateringBasedOnAlarm)
  //      {
  //        insertSql.concat("water based on alarm");
  //      }
  //      else
  //      {
  //        insertSql.concat(soilMsg);
  //      }
  insertSql.concat("');");

  return insertSql;
}



void loop()
{ //

  // Reading temperature or humidity takes about 250 milliseconds!
  delay(250);
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  indoorHumidity = dhtDrwRom.readHumidity();
  // Read temperature as Celsius
  indorTempinC = dhtDrwRom.readTemperature();
  // Read temperature as Fahrenheit
  indorTempinF = dhtDrwRom.readTemperature(true);
  // Compute heat index, Must send in temp in Fahrenheit!
  hi = dhtDrwRom.computeHeatIndex(indorTempinF, indoorHumidity);
  hIinCel = (hi + 40) / 1.8 - 40;
  dP = (Dewpnt_heatIndx::dewPointFast(indorTempinC, indoorHumidity));
  dPF = ((dP * 9) / 5) + 32;

  // outoor DHT sensor readings
  outoorHumidity = dhtOutdoor.readHumidity();
  // Read temperature as Celsius
  outdoorTempInC = dhtOutdoor.readTemperature();
  // Read temperature as Fahrenheit
  outdoorTempInF = dhtOutdoor.readTemperature(true);
  // Compute outdoor heat index, Must send in temp in Fahrenheit!
  outdoorhIinFah = dhtOutdoor.computeHeatIndex(outdoorTempInF, outoorHumidity);
  outdoorhIinCel = (outdoorhIinFah + 40) / 1.8 - 40;
  outdoordP = (Dewpnt_heatIndx::dewPointFast(outdoorTempInC, outoorHumidity));
  outdoordPF = ((outdoordP * 9) / 5) + 32;

  buttonState = digitalRead(buttonPin);
  currentMillis = millis();   // capture the latest value of millis()

  //  int watrLvlSnsr = analogRead(pwatLvlSense);
  int rainSenseReading = analogRead(rainsense);
  wateringBasedOnAlarm = waterThePlant();
  // if not time to water plant check soil moisture
  if (!wateringBasedOnAlarm) {
    int hr = util::getHour();
    // Do not water plant at night
    if ((hr >= 19  && hr <= 23) || (hr >= 0 && hr <= 7))
    {
      power_to_solenoid = false;
      soilMsg = "turned off at night";
    } // if it is day check if soil is dry or not.
    else if  (avgVal < DRY_SOIL_DEFAULT )
    {
      soilMsg = "Soil is wet.";
    }
    else
    {
      // Sensor detected average value is greater than DRY_SOIL_DEFAULT,
      // Soil is dry. so first check is it raining.
      if (rainSenseReading > 500) {
        rainMsg = F("Not Raining.");
        power_to_solenoid = true;
      }
      else // if (rainSenseReading < 500)
      {
        power_to_solenoid = false;
        rainMsg = F("It is raining heavily !");
        soilMsg = "turned off when raining";
        if (rainSenseReading < 330)
        {
          rainMsg = F("Moderate rain.");
        }
        else if (rainSenseReading < 200)
        {
          rainMsg = F("Light Rain Showers !");
        }
      }
      //      if (watrLvlSnsr >= 400)
      //      {
      //        power_to_solenoid = false;
      //        soilMsg = "Pot full of water";
      //      }
      // else power_to_solenoid = false;

    }
    // Turn on Solenoid Valve and water the plants
    if (power_to_solenoid == true)
    { // i.e. if onBoardLedState is HIGH
      soilMsg = "Watering plant";
      // turn solenoid off after 45 sec

      //       digitalWrite(solenoidPin, HIGH);

      // if the Solenoid is on, we must wait for the duration to expire before turning it off
      if (currentMillis - previousOnBoardLedMillis >= blinkDuration) {
        // time is up, so change the state to LOW
        power_to_solenoid = false; // don't execute this again
        digitalWrite(solenoidPin, LOW);
        soilMsg = "Stopped Watering";
      }
      // and save the time when we made the change
      previousOnBoardLedMillis += blinkDuration;
    }
    else
    {
      power_to_solenoid = false; // don't execute this again
      digitalWrite(solenoidPin, LOW);
    }
  }
  //If button pressed...
  if (buttonState == LOW) {
    Serial.println(buttonState, DEC);
    //...ones, turn solenoid on!
    if ( flag == 0) {
      digitalWrite(solenoidPin, HIGH);
      //      digitalWrite(buzzerout, HIGH);
      flag = 1; //change flag variable
    }
    //...twice, turn solenoid off!
    else if ( flag == 1) {
      digitalWrite(solenoidPin, LOW);
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
          // Ajax request - send Json file
          if (util::StrContains(HTTP_req, "json") or util::StrContains(HTTP_req, "json?format=json")) {
            // send rest of HTTP header
            // client.println("Content-Type: application/json;charset=utf-8");
            client.println(F("Content-Type: text/html"));
            client.println("Access-Control-Allow-Origin: *");
            client.println("Keep-Alive: timeout=2, max=100");
            client.println("Connection: keep-alive");
            client.println();
            outputJson(client, true);
          } else if (util::StrContains(HTTP_req, "/?waterPlant1")) {
            soilMsg = "Manual watering";
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

            client.println(F("<!DOCTYPE HTML>"));
            client.println(F("<html><head><title>"));
            client.println(F("Welcome to Arduino Mega WebServer</title>"));
            client.println(F("<meta name='apple-mobile-web-app-capable' content='yes' />"));
            client.println(F("<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent' />"));
            client.println("<!-- link rel='stylesheet' type='text/css' href='http://randomnerdtutorials.com/ethernetcss.css'-->");
            util::printProgStr(client, util::htmlStyleMultiline);
            client.println(F("</head><body style='background-color:grey'>"));
            client.println(c);
            client.print(F("<p style='color:red';style='font-family: Arial'> LIVE: </p>"));
            util::displayTime(client);
            if (rainSenseReading < 500) {
              client.print(F("<hr><br>Rain Sensor reads: "));
              client.print(rainSenseReading);
              client.print(" ");
              client.println(rainMsg);
            }
            client.print(F("<hr><br>Outdoor Temperature: <u>+</u>"));
            client.print(outdoorTempInC, 1);
            client.print(F("&#8451; / "));
            client.print(outdoorTempInF);
            client.println(F("&#8457;, Humidity: "));
            client.print(outoorHumidity, 1);
            client.print(F("%<br>Dew Point: "));
            client.print(outdoordP);
            client.print(F("&#8451; Heat Index: "));

            client.print(outdoorhIinCel);
            client.print(F("&#8451;/ "));
            client.print(outdoorhIinFah);
            client.println(F("&#8457; <br>"));


            // --------------- Drawing Room ------------------//
            client.print("<br>Indoor temperature: ");
            client.print(indorTempinC);
            client.print("&#8451;/ ");
            client.println(indorTempinF);
            client.println(F(" &#8457;, Humidity: "));
            client.print(indoorHumidity, 1);
            client.print(F("%<br>Dew Point: "));
            client.print(dP);
            client.print(F("&#8451; Heat Index: "));
            client.print(hIinCel);
            client.print(F("&#8451;/ "));
            client.print(hi);
            client.println(F("&#8457; <br><hr>"));


            //            if (analogValue < 10) {
            //              client.println(F("<br><div class='tooltip'>It is  <font style='color:red';>dark</font>."));
            //            }
            //            else if (analogValue > 10 && analogValue < 50) {
            //              client.println(F("<br><div class='tooltip'>Fair amount of<font style='color:yellow';><b>light</b></font>, but not very bright"));
            //            }
            //            else if (analogValue >= 50 && analogValue <= 100) {
            //              client.println(F("<br><div class='tooltip'>Fair amount of <font style='color:yellow';><b>light</b></font>, but not bright enough."));
            //            }
            //            else {
            //              client.println(F("<br><div class='tooltip'>It is Full Bright Day<font style='color:green';><b> :) </b></font>."));
            //            }
            //
            //            client.print(F("<span class='tooltiptext'> LDR Value Reads: "));
            //            client.print(analogValue);
            //            client.println(F("</span></div>"));

            client.print(F("<br>Pot Soil Moisture: "));
            client.print(val);
            client.print(F(". Average Soil Moisture: "));
            client.print(avgVal);
            client.print(F("<br><a href=\"/?waterPlant1\"\">Water the plant in pot.</a>"));
            client.println(soilMsg);
            if (wateringBasedOnAlarm) {
              client.println(F("<br><br>Watering plant based on set alarm ."));
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
  if ((millis() - timer) > sqlInsertInterval) {

    // if the soil sensor is off turn it on and vice-versa:
    if (soilSensorState == LOW) {
      soilSensorState = HIGH;

      // read from the sensor:
      val = analogRead(SENSE);
      // condition when sensor is taken out
      if (val < 30) {
        // reset for new values
        myRA.clear();
      }

      myRA.addValue(val);

      // get the average as well after seconds in interval.
      avgVal = myRA.getAverage();

      // set the soil moisture sensor with the ledState of the variable
      digitalWrite(power_soilMoist_pin, soilSensorState);


      //      ICMPEchoReply echoReply = ping(server_addr, 4);
      //      if (echoReply.status == SUCCESS)
      //      {
      // timed out
      timer += sqlInsertInterval;// reset timer by moving it along to the next interval

      if (!connected) {
        Serial.println("Establishing Connection");
        my_conn.mysql_connect(server_addr, 3306, user, password);
        connected = true;
      }
      else if (connected == true)
      {
        insertSql = getSqlInsertString();
        const char *mycharp = insertSql.c_str();
        //      my_conn.cmd_query("use arduinoSensorData;");
        //      Serial.print("Inserting : ");
        //      Serial.println(insertSql);
        my_conn.cmd_query(mycharp);
        Serial.println("Connection Successfull,inserting to database.");
        sqlInsertInterval = 60000; // set the repeat interval to a  minute after first insertion.
      }
      else {
        Serial.println("Connection failed.");
      }
      //      }
    } else
    {
      soilSensorState = LOW;
    }
  }
}

