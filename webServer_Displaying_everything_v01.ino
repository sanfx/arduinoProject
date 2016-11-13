
#include <SPI.h>
#include <Ethernet.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

int greenLedPin = 2;
int yellowLedPin = 3;
int redLedPin = 4;

int lightSensorPin = A0;
int analogValue = 0;


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

          analogValue = analogRead(lightSensorPin);
          client.print("Light Sensor reads ");
          client.print(analogValue);
          if (analogValue < 50) {
            client.println(", Light is <b>off</b>");
            digitalWrite(redLedPin, HIGH);

          }
          else if (analogValue >= 50 && analogValue <= 100) {
            client.println(", Light is <b>on</b>, but not bright enough");
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
          client.println("</html>");
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
