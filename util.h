#include <dht.h>
//#include <cstring.h>
namespace util
{

double* getTempHumdata(const int sensorPin)
{
  double old_data [2];
  dht DHT;
  double nums [2];
  int chk = DHT.read11(sensorPin);
  //    int chk = DHT.read(sensorPin);
  Serial.print(chk);
  int hum;
  double temp;
  Serial.print("<-- Sensor ");
  Serial.print(sensorPin);
  Serial.print(" ");

  switch (chk)
  {
    case 0:
      hum = (double) DHT.humidity;
      temp = (double) DHT.temperature;
      nums[0] = temp;
      nums[1] = hum;
      memcpy(nums, old_data, sizeof(nums));
      Serial.print("Temperature: ");
      Serial.print(nums[0]);
      Serial.print(" Humidity: ");
      Serial.println(nums[1]);
      Serial.print(hum);
      Serial.print(" % ");
      Serial.print(temp);
      Serial.println(" &#8451");
      break;
    case -1:
      Serial.println(" Checksum error");
      return old_data;
      break;

    case -2: Serial.println(" Time out error"); break;
    default: Serial.println(" Unknown error"); break;
      return nums;
  }
}
}
