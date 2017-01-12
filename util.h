//#include <dht.h>
#include "Wire.h"
//#include <cstring.h>
namespace util
{


// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
  char found = 0;
  char index = 0;
  char len;

  len = strlen(str);

  if (strlen(sfind) > len) {
    return 0;
  }
  while (index < len) {
    if (str[index] == sfind[found]) {
      found++;
      if (strlen(sfind) == found) {
        return 1;
      }
    }
    else {
      found = 0;
    }
    index++;
  }

  return 0;
}

void printProgStr(EthernetClient client, PGM_P str)
{
  char c;
  if (!str) return;
  while ((c = pgm_read_byte(str++)))
    client.print(c);
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return ( (val / 16 * 10) + (val % 16) );
}



//void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte
//                   dayOfMonth, byte month, byte year)
//{
//  // sets time and date data to DS3231
//  Wire.beginTransmission(DS3231_I2C_ADDRESS);
//  Wire.write(0); // set next input to start at the seconds register
//  Wire.write(decToBcd(second)); // set seconds
//  Wire.write(decToBcd(minute)); // set minutes
//  Wire.write(decToBcd(hour)); // set hours
//  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
//  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
//  Wire.write(decToBcd(month)); // set month
//  Wire.write(decToBcd(year)); // set year (0 to 99)
//  Wire.endTransmission();
//}

void readDS3231time(byte *second,
                    byte *minute,
                    byte *hour,
                    byte *dayOfWeek,
                    byte *dayOfMonth,
                    byte *month,
                    byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}


//
//double* getTempHumdata(const int sensorPin)
//{
//  double old_data [2];
//  dht DHT;
//  double nums [2];
//  int chk = DHT.read11(sensorPin);
//  //    int chk = DHT.read(sensorPin);
//  Serial.print(chk);
//  int hum;
//  double temp;
//  Serial.print("<-- Sensor ");
//  Serial.print(sensorPin);
//  Serial.print(" ");
//
//  switch (chk)
//  {
//    case 0:
//      hum = (double) DHT.humidity;
//      temp = (double) DHT.temperature;
//      nums[0] = temp;
//      nums[1] = hum;
//      memcpy(nums, old_data, sizeof(nums));
//      Serial.print("Temperature: ");
//      Serial.print(nums[0]);
//      Serial.print(" Humidity: ");
//      Serial.println(nums[1]);
//      Serial.print(hum);
//      Serial.print(" % ");
//      Serial.print(temp);
//      Serial.println(" &#8451");
//      break;
//    case -1:
//      Serial.println(" Checksum error");
//      return old_data;
//      break;
//
//    case -2: Serial.println(" Time out error"); break;
//    default: Serial.println(" Unknown error"); break;
//      return nums;
//  }
//}



void displayTime(EthernetClient client)
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                 &year);
  client.print("Today is ");
  switch (dayOfWeek) {
    case 1:
      client.println("Sunday,");
      break;
    case 2:
      client.println("Monday,");
      break;
    case 3:
      client.println("Tuesday,");
      break;
    case 4:
      client.println("Wednesday,");
      break;
    case 5:
      client.println("Thursday,");
      break;
    case 6:
      client.println("Friday,");
      break;
    case 7:
      client.println("Saturday,");
      break;
  }

  client.print(" ");
  client.print(dayOfMonth, DEC);
  client.print("/");
  client.print(month, DEC);
  client.print("/");
  client.print(year, DEC);
  client.print(" Page was last loaded at: ");
  const String msg = "";
  // send it to the client
  if (hour > 12 )
  {
    hour = hour - 12;
    msg = " PM";
  }
  else
  {
    hour = hour;
    msg = " AM";
  }

  client.print(hour, DEC);
  // convert the byte variable to a decimal number when displayed
  client.print(":");
  if (minute < 10)
  {
    client.print("0");
  }
  client.print(minute, DEC);

  client.print(msg);

}

//int* getTime(size_t& arraySize)
//{
//  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
//  // retrieve data from DS3231
//  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
//                       &year);
//
//    int time[] = {hour, minute, second};
//    return time;
//}



int getHour()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                       &year);
  return hour;
}


int getMinute()
{
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                       &year);
  return minute;
}


}
