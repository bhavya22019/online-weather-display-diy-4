/********************************************************************************************************************
 *  This sketch obtains online weather information from the OpenWeatherMap service and displays it to an OLED module
 *  Written for the Arduino Nano 33 IoT Board and 0.96" I2C OLED Module
 *
 *  By Frenoy Osburn
 *  YouTube Video: https://youtu.be/-62JGZm2DrA
 *********************************************************************************************************************/
 
 /********************************************************************************************************************
 *  Useful Links:
 *  https://openweathermap.org/
 *  ArduinoJSON Assistant: https://arduinojson.org/v6/assistant/
 *  URL: api.openweathermap.org/data/2.5/weather?q=Edinburgh,uk&units=metric&APPID=YOUR_API_KEY
 *********************************************************************************************************************/
 
#include <SPI.h>
#include <WiFiNINA.h>
#include <stdio.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "arduino_secrets.h"

#define SUN  0
#define SUN_CLOUD  1
#define CLOUD 2
#define RAIN 3
#define THUNDER 4

U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display

char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;

// Initialize the Wifi client library
WiFiSSLClient client;

unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 30L * 1000L; // delay between updates, in milliseconds

void setup()
{  
  u8g2.begin();
  u8g2.enableUTF8Print();

  Serial.begin(9600);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) 
  {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") 
  {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) 
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  // you're connected now, so print out the status:
  printWifiStatus();
}

void loop() 
{    
  if (millis() - lastConnectionTime > postingInterval) 
  {
    httpRequest();
  }
}

void httpRequest()
{    
  // if there's a successful connection:
  if (client.connect("api.openweathermap.org", 443)) 
  {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET /data/2.5/weather?q=edinburgh,uk&units=metric&APPID=YOUR_API_KEY HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("Connection: close");
    client.println();

    // Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) 
    {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return;
    }

    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) 
    {
      Serial.println(F("Invalid response"));
      return;
    }

    // Allocate the JSON document
    // Use arduinojson.org/v6/assistant to compute the capacity.
    const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(0) + 2*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + 2*JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(14) + 280;
    DynamicJsonDocument doc(capacity);
    
    // Parse JSON object
    DeserializationError error = deserializeJson(doc, client);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
        
    int weatherId = doc["weather"][0]["id"].as<int>();
    float weatherTemperature = doc["main"]["temp"].as<float>();
    int weatherHumidity = doc["main"]["humidity"].as<int>();
    
    //Disconnect
    client.stop();
    
    Serial.println(F("Response:"));
    Serial.print("Weather: ");
    Serial.println(weatherId);
    Serial.print("Temperature: ");
    Serial.println(weatherTemperature);
    Serial.print("Humidity: ");
    Serial.println(weatherHumidity);
    Serial.println();
    
    char scrollText[15];
    sprintf(scrollText, "Humidity:%3d%%", weatherHumidity);

    if(weatherId == 800)    //clear
    {
      draw(scrollText, SUN, weatherTemperature);
    }
    else
    {
      switch(weatherId/100)
      {
        case 2:     //Thunderstorm
            draw(scrollText, THUNDER, weatherTemperature);
            break;
    
        case 3:     //Drizzle
        case 5:     //Rain
            draw(scrollText, RAIN, weatherTemperature);
            break;
    
        case 7:     //Sun with clouds
            draw(scrollText, SUN_CLOUD, weatherTemperature);
            break;
        case 8:     //clouds
            draw(scrollText, CLOUD, weatherTemperature);
            break;
        
        default:    //Sun with clouds           
            draw(scrollText, SUN_CLOUD, weatherTemperature);
            break;
      }    
    }
        
    // note the time that the connection was made:
    lastConnectionTime = millis();
  } 
  else 
  {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}

void printWifiStatus() 
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void drawWeatherSymbol(u8g2_uint_t x, u8g2_uint_t y, uint8_t symbol)
{
  // fonts used:
  // u8g2_font_open_iconic_embedded_6x_t
  // u8g2_font_open_iconic_weather_6x_t
  // encoding values, see: https://github.com/olikraus/u8g2/wiki/fntgrpiconic
  
  switch(symbol)
  {
    case SUN:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 69);  
      break;
    case SUN_CLOUD:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 65); 
      break;
    case CLOUD:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 64); 
      break;
    case RAIN:
      u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
      u8g2.drawGlyph(x, y, 67); 
      break;
    case THUNDER:
      u8g2.setFont(u8g2_font_open_iconic_embedded_6x_t);
      u8g2.drawGlyph(x, y, 67);
      break;      
  }
}

void drawWeather(uint8_t symbol, int degree)
{
  drawWeatherSymbol(0, 48, symbol);
  u8g2.setFont(u8g2_font_logisoso32_tf);
  u8g2.setCursor(48+3, 42);
  u8g2.print(degree);
  u8g2.print("°C");   // requires enableUTF8Print()
}

/*
  Draw a string with specified pixel offset. 
  The offset can be negative.
  Limitation: The monochrome font with 8 pixel per glyph
*/
void drawScrollString(int16_t offset, const char *s)
{
  static char buf[36];  // should for screen with up to 256 pixel width 
  size_t len;
  size_t char_offset = 0;
  u8g2_uint_t dx = 0;
  size_t visible = 0;
  len = strlen(s);
  if ( offset < 0 )
  {
    char_offset = (-offset)/8;
    dx = offset + char_offset*8;
    if ( char_offset >= u8g2.getDisplayWidth()/8 )
      return;
    visible = u8g2.getDisplayWidth()/8-char_offset+1;
    strncpy(buf, s, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_8x13_mf);
    u8g2.drawStr(char_offset*8-dx, 62, buf);
  }
  else
  {
    char_offset = offset / 8;
    if ( char_offset >= len )
      return; // nothing visible
    dx = offset - char_offset*8;
    visible = len - char_offset;
    if ( visible > u8g2.getDisplayWidth()/8+1 )
      visible = u8g2.getDisplayWidth()/8+1;
    strncpy(buf, s+char_offset, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_8x13_mf);
    u8g2.drawStr(-dx, 62, buf);
  }
}

void draw(const char *s, uint8_t symbol, int degree)
{
  int16_t offset = -(int16_t)u8g2.getDisplayWidth();
  int16_t len = strlen(s);
  for(;;)
  {
    u8g2.firstPage();
    do {
      drawWeather(symbol, degree);      
      drawScrollString(offset, s);
    } while ( u8g2.nextPage() );
    delay(20);
    offset+=2;
    if ( offset > len*8+1 )
      break;
  }
}
