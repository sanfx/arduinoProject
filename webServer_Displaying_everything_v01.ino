#include <SFE_BMP180.h>
#include <SPI.h>
#include <Ethernet.h>

#include<dht.h>
dht DHT;

#define DHT11_PIN 8 // Second pin, leave 3rd pin unconnected 4th is ground

// You will need to create an SFE_BMP180 object, here called "pressure":
SFE_BMP180 bmp;

#define ALTITUDE 920.0 // Altitude of Bangalore in Karnataka, India. in meters
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

String HTTP_req;          // stores the HTTP request
boolean LED_status = 0;   // state of LED, off by default

int greenLedPin = 2; // LED on pin 2
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
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  if (bmp.begin()) {
    Serial.println("BMP init Success");
  }

  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

  pinMode(greenLedPin, OUTPUT); // LED on pin 2
  pinMode(yellowLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);

}

void loop() {
  char status;
  //  double P, p0, a;
  status = bmp.startTemperature();

  int chk = DHT.read11(DHT11_PIN);
  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.

  status = bmp.startTemperature();
  if (status != 0)
  {

    //    delay(status);
    status = bmp.getTemperature(tempInC);

  }
  else Serial.println("error retrieving temperature measurement\n");

  humidity = DHT.humidity;
  dP = (dewPointFast(tempInC, humidity));
  dPF = ((dP * 9) / 5) + 32;
  tF = ((tempInC * 9) / 5) + 32;
  analogValue = analogRead(lightSensorPin);
  if (analogValue < 50) {
    digitalWrite(redLedPin, HIGH);
  }
  else if (analogValue >= 50 && analogValue <= 100) {
    digitalWrite(yellowLedPin, HIGH);
  }
  else {
    digitalWrite(greenLedPin, HIGH);
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
        HTTP_req += c;  // save the HTTP request 1 char at a time
        Serial.println(c);
        if (c == '\n' && currentLineIsBlank) {

          if (!sentHeader) {
            // send a standard http response header
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");  // the connection will be closed after completion of the response
            client.println("Refresh: 10");  // refresh the page automatically every 5 sec
            client.println();
            sentHeader = true;
          }

          client.println("<!DOCTYPE HTML>");
          client.println("<html><head><title>");
          client.println("Welcome to Arduino WebServer</title>");
          client.println("<body style='background-color:grey'>");
          client.println(c);
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
          client.print("&#8451; Heat Index: ");

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

          client.print("Light Sensor reads ");
          client.print(analogValue);
          if (analogValue < 10) {
            client.println(", Light is <font style='color:red';>off</font>");
          }
          else if (analogValue > 10 && analogValue < 50) {
            client.println(", Light is Light is <font style='color:yellow';><b>on</b></font>, but very dim");
          }
          else if (analogValue >= 50 && analogValue <= 100) {
            client.println(", Light is <font style='color:yellow';><b>on</b></font>, but not bright enough.");
          }
          else {
            client.println(", Light is <font style='color:green';><b>on</b></font>.");
          }

          Serial.print("Analogue Value ");
          Serial.println(analogValue);

          //          client.println("<h1>LED</h1>");
          //          client.println("<p>Click to switch LED on and off.</p>");
          //          client.println("<form method=\"get\">");
          //          ProcessCheckbox(client);
          //          client.println("</form>");

          client.println("</body></html>");
          Serial.print(HTTP_req);
          HTTP_req = "";    // finished with request, empty string
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
  //  delay(200);
  //  digitalWrite(greenLedPin, LOW);
  //  digitalWrite(yellowLedPin, LOW);
  //  digitalWrite(redLedPin, LOW);
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


// switch LED and send back HTML for LED checkbox
void ProcessCheckbox(EthernetClient cl)
{
  if (HTTP_req.indexOf("LED2=2") > -1) {  // see if checkbox was clicked
    // the checkbox was clicked, toggle the LED
    if (LED_status) {
      LED_status = 0;
    }
    else {
      LED_status = 1;
    }
  }

  if (LED_status) {    // switch LED on
    digitalWrite(greenLedPin, HIGH);
    // checkbox is checked
    cl.println("<input type=\"checkbox\" name=\"LED2\" value=\"2\" \
        onclick=\"submit();\" checked>LED2");
  }
  else {              // switch LED off
    digitalWrite(greenLedPin, LOW);
    // checkbox is unchecked
    cl.println("<input type=\"checkbox\" name=\"LED2\" value=\"2\" \
        onclick=\"submit();\">LED2");
  }
}
