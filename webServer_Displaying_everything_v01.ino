
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>
#include<dht.h>
dht DHT;

#define DHT11_PIN 8 // Second pin, leave 3rd pin unconnected 4th is ground


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

/* ******** NTP Server Settings ******** */
/* us.pool.ntp.org NTP server
   (Set to your time server of choice) */
IPAddress timeServer(216, 23, 247, 62);

/* Set this to the offset (in seconds) to your local time
   This example is GMT +530 IST */
const long timeZoneOffset = -19800L;

/* Syncs to NTP server every 15 seconds for testing,
   set to 1 hour or more to be reasonable */
unsigned int ntpSyncTime = 3600;


/* ALTER THESE VARIABLES AT YOUR OWN RISK */
// local port to listen for UDP packets
unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE = 48;
// Buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
// Keeps track of how long ago we updated the NTP server
unsigned long ntpLastUpdate = 0;
// Check last time clock displayed (Not in Production)
time_t prevDisplay = 0;

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(8090);

int greenLedPin = 2;
int yellowLedPin = 3;
int redLedPin = 4;

int lightSensorPin = A0;
int analogValue = 0;


double tempInC;
int humidity;
float dP; // dew point
float dPF; // dew point in fahrenheit
float tF; // temperature in fahrenheit

double hIinFah;
double hIinCel;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }


  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);

}


void loop() {
  int chk = DHT.read11(DHT11_PIN);
  tempInC = DHT.temperature;
  humidity = DHT.humidity;
  dP = (dewPointFast(tempInC, humidity));
  dPF = ((dP * 9) / 5) + 32;
  tF = ((tempInC * 9) / 5) + 32;

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    //    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 10");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<body style='background-color:#E6E6FA'>");
          client.println("<p style='color:red';style='font-family: Arial'>LIVE</p>");
          client.print("Room Temperature: <u>+</u>");

          client.print(tempInC, 1);
          client.print("&#8451; / ");
          client.print(tF);
          client.println(" &#8457;,");
          client.print(" Humidity: ");

          client.print(humidity, 1);
          client.print("%<br>");
          client.print("Dew Point: ");
          client.print(dP);
          client.print(" &#8451; ");
          client.print("Heat Index: ");

          // Heat Index calculation only valid if temperature is 80F or above
          // and the humidity is 40% or above - Print tF if not true
          //  Serial.print("tF =");
          //  Serial.print(tF);

          if (tF < 70 || humidity < 30) {
            client.print(tempInC);
            client.print(" &#8451; / ");
            client.print(tF);
            client.println(" &#8457;");
          }
          else {
            hIinFah = heatIndex(tF, humidity);
            hIinCel = (hIinFah + 40) / 1.8 - 40;
            client.print(hIinCel);
            client.print(" &#8451;/ ");
            client.print(hIinFah);
            client.println(" &#8457;");
          }
          client.println("<br>");

          analogValue = analogRead(lightSensorPin);
          client.print("Light Sensor reads ");
          client.print(analogValue);
          if (analogValue < 10) {
            client.println(", Light is <b>off</b>");
            digitalWrite(redLedPin, HIGH);

          }
          else if (analogValue > 10 && analogValue < 50) {
            client.println(", Light is <b>on</b>, but very dim");
          }
          else if (analogValue >= 50 && analogValue <= 100) {
            client.println(", Light is <b>on</b>, but not bright enough.");
            digitalWrite(yellowLedPin, HIGH);
          }
          else {

            client.println(", Light is <b>on</b> and bright enough.");
            digitalWrite(greenLedPin, HIGH);
          }
          Serial.print("Analogue Value ");
          Serial.println(analogValue);
          //          delay(200);
          digitalWrite(greenLedPin, LOW);
          digitalWrite(yellowLedPin, LOW);
          digitalWrite(redLedPin, LOW);
          client.println("</body></html>");
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

// reference: http://en.wikipedia.org/wiki/Dew_point
double dewPointFast(double celsius, double humidity)
{
  double a = 17.271;
  double b = 237.7;
  double temp = (a * celsius) / (b + celsius) + log(humidity * 0.01);
  double Td = (b * temp) / (a - temp);
  return Td;
}

double heatIndex(double tempF, double humidity)
{
  double c1 = -42.38, c2 = 2.049, c3 = 10.14, c4 = -0.2248, c5 = -6.838e-3, c6 = -5.482e-2, c7 = 1.228e-3, c8 = 8.528e-4, c9 = -1.99e-6  ;
  double T = tempF;
  double R = humidity;

  double A = (( c5 * T) + c2) * T + c1;
  double B = ((c7 * T) + c4) * T + c3;
  double C = ((c9 * T) + c8) * T + c6;

  double rv = (C * R + B) * R + A;
  return rv;
}
