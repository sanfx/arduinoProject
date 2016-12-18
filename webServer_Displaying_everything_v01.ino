
#include <SPI.h>
#include "Wire.h"
#define DS3231_I2C_ADDRESS 0x68

#include <Ethernet.h>
#include <elapsedMillis.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Dewpnt_heatIndx.h"
#include "util.h"
#include <dht.h>
dht DHT;

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



int getHour()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  util::readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                       &year);
  return hour;
}




bool power_to_solenoid = false;

#define ONE_WIRE_BUS 42 /*-(Connect to Pin 42 )- brown wire out on potentiometer*/
/* Set up a oneWire instance to communicate with any OneWire device*/
OneWire ourWire(ONE_WIRE_BUS);

/* Tell Dallas Temperature Library to use oneWire Library */
DallasTemperature tempSensor(&ourWire);

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 8090 is default for HTTP):
EthernetServer server(8090);

String rainMsg;

// if val of soil moisture is greater than 30 turn on solenoid
const int8_t DRY_SOIL_DEFAULT = 25;

// ///////////// PIN Setup /////////////////
const int solenoidPin = 6;    // D6 : This is the output pin on the Arduino we are using
#define DHT11_PIN 8 // D8 to 2nd pin on DHT, leave 3rd pin unconnected 4th is gnd
const int8_t rainsense = 7; // analog sensor input pin A7
const int buzzerout = 2; // digital output pin D2 - buzzer output
const int8_t pwatLvlSense = A2; // Pot Water Level Sensor A2 connected with blue wire
int SENSE = 1; // Soil Sensor input at Analog PIN A1 Green wire
int val = 0;

int lightSensorPin = A3; // earlier it was A8
int analogValue = 0;

double tempInC;
int humidity;
float dP; // dew point
float dPF; // dew point in fahrenheit
float tF; // temperature in fahrenheit

double hIinFah;
double hIinCel;

String soilMsg;

elapsedMillis timer0;

// the interval in mS
#define interval 45000

void setup() {
  Wire.begin();

  power_to_solenoid = false;
  timer0 = 0; // clear the timer at the end of startup
  pinMode(solenoidPin, OUTPUT); // Sets the pin as an output
  pinMode(buzzerout, OUTPUT);
  pinMode(rainsense, INPUT);
  tempSensor.begin();
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB port only
  }

  Serial.print(F("server is at "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  int watrLvlSnsr = analogRead(pwatLvlSense);
  val = analogRead(SENSE);
  val = val / 10;

  int rainSenseReading = analogRead(rainsense);

  //  tempInC = util::getTempHumdata(DHT11_PIN)[0];
  //  humidity = util::getTempHumdata(DHT11_PIN)[1];
  int chk = DHT.read11(DHT11_PIN);
  tempSensor.requestTemperatures(); // Send the command to get temperatures
  tempInC = tempSensor.getTempCByIndex(0);
  tF = tempSensor.getTempFByIndex(0);
  humidity = DHT.humidity;
  dP = (Dewpnt_heatIndx::dewPointFast(tempInC, humidity));
  dPF = ((dP * 9) / 5) + 32;
  analogValue = analogRead(lightSensorPin);
  Serial.print(F("LDR value: "));
  Serial.print(analogValue);
  //  digitalWrite (redLedPin, analogValue < 55) ;
  //  digitalWrite (yellowLedPin, analogValue >= 55 && analogValue <= 100) ;
  //  digitalWrite (greenLedPin, analogValue > 100) ;
  Serial.print(F(", Soil Moisture: "));
  Serial.println(val);

  if (rainSenseReading < 500)  {
    rainMsg = F("It is raining heavily !");
    power_to_solenoid = false;
  }
  if (rainSenseReading < 300) {
    rainMsg = F("Moderate rain.");
    power_to_solenoid = false;
  }
  if (rainSenseReading < 200) {
    rainMsg = F("Light Rain Showers !");
    power_to_solenoid = false;
  }
  if (rainSenseReading > 500) {
    rainMsg = F("Not Raining.");
    power_to_solenoid = true;
  }
  else power_to_solenoid = false;

  if ((getHour() >= 19  && getHour() <= 23) || (getHour() >= 0 && getHour() <= 7)) {
    power_to_solenoid = false;
  }

  if (watrLvlSnsr >=400){
    power_to_solenoid = false;
  }

  // Turn on Solenoid Valve if soil moisture value less than 25
  if (val < DRY_SOIL_DEFAULT && power_to_solenoid == true) {
    soilMsg = F("Soil is dry. ");
    // do not water pot if its raining or if it is night.
    if (analogValue > 300) {
      soilMsg = soilMsg +  F("Watering the plant.");
      // turn solenoid off after 45 sec
      digitalWrite(solenoidPin, HIGH);
      digitalWrite(buzzerout, HIGH);
      if ((!power_to_solenoid) && (timer0 > interval)) {
        power_to_solenoid = false; // don't execute this again
        digitalWrite(solenoidPin, LOW);
        digitalWrite(buzzerout, LOW);

      }

    }
  }
  else {
    digitalWrite(solenoidPin, LOW);
    soilMsg = F("Soil is damp.");
    power_to_solenoid = false;
    digitalWrite(buzzerout, LOW);
  }

  // listen for incoming clients
  EthernetClient client = server.available();

  if (client) {

    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    boolean sentHeader = false;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
        if (c == '\n' && currentLineIsBlank) {

          if (!sentHeader) {
            // send a standard http response header
            client.println(F("HTTP/1.1 200 OK"));
            client.println(F("Content-Type: text/html"));
            client.println(F("Connection: close"));  // the connection will be closed after completion of the response
            client.println(F("Refresh: 10"));  // refresh the page automatically every 5 sec
            client.println();
            sentHeader = true;
          }

          client.println(F("<!DOCTYPE HTML>"));
          client.println(F("<html><head><title>"));
          client.println(F("Welcome to Arduino WebServer</title>"));
          //          util::printProgStr( htmlStyleMultiline );
          client.println(F("</head><body style='background-color:grey'>"));
          client.println(c);
          client.println(F("<p style='color:red';style='font-family: Arial'>LIVE</p>"));

          if (rainSenseReading < 500) {
            client.print(rainMsg);
            client.print(F(" Rain Sensor reads: "));
            client.println(rainSenseReading);
          }
          else client.println();
          client.print(F("<br>Temperature: <u>+</u>"));
          client.print(tempInC, 1);
          client.print(F("&#8451; / "));
          client.print(tF);
          client.println(F(" &#8457;, Humidity: "));

          client.print(humidity, 1);
          client.print(F("%<br>Dew Point: "));
          client.print(dP);
          client.print(F("&#8451; Heat Index: "));

          if (tF < 70 || humidity < 30) {
            client.print(tempInC);
            client.print(F(" &#8451; / "));
            client.print(tF);
            client.println(F(" &#8457;"));
          }
          else {
            hIinFah = Dewpnt_heatIndx::heatIndex(tF, humidity);
            hIinCel = (hIinFah + 40) / 1.8 - 40;
            client.print(hIinCel);
            client.print(F(" &#8451;/ "));
            client.print(hIinFah);
            client.println(F(" &#8457; <br>"));
          }

          if (analogValue < 10) {
            client.println(F("<div class='tooltip'>It is  <font style='color:red';>dark</font>."));
          }
          else if (analogValue > 10 && analogValue < 50) {
            client.println(F("<div class='tooltip'>Fair amount of<font style='color:yellow';><b>light</b></font>, but not very bright"));
          }
          else if (analogValue >= 50 && analogValue <= 100) {
            client.println(F("<div class='tooltip'>Fair amount of <font style='color:yellow';><b>light</b></font>, but not bright enough."));
          }
          else {
            client.println(F("<div class='tooltip'>Full Bright Day<font style='color:green';><b> :) </b></font>."));
          }

          client.print(F("<span class='tooltiptext'> LDR Value Reads: "));
          client.print(analogValue);
          client.println(F("</span>"));
          client.print(F("<br>Pot Soil Moisture: "));
          client.print(val);
          client.print(F("&nbsp;"));
          client.println(soilMsg);
          client.println (F("</body></html>"));

          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }

    }

    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }

}

