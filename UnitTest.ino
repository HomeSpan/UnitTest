/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2022 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/

#include <Wire.h>
#include "FeatherPins.h"

#include "HomeSpan.h"
#include "extras/Pixel.h"
#include "extras/PwmPin.h"

#define CONTROL_PIN   F25
#define STATUS_PIN    F26
#define LED_RED_PIN   F33
#define LED_BLUE_PIN  F32
#define LED_GREEN_PIN F14
#define LED_BUTTON    F4
#define PIXEL_PIN     F27

#if SOC_TOUCH_SENSOR_NUM > 0
  #define PIXEL_BUTTON  F12
#else
  #define PIXEL_BUTTON  -1
#endif

// Create Custom Characteristics to save a "favorite" HSV color

CUSTOM_CHAR(FavoriteHue, 00000001-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 360, false);
CUSTOM_CHAR(FavoriteSaturation, 00000002-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 100, false);
CUSTOM_CHAR(FavoriteBrightness, 00000003-0001-0001-0001-46637266EA00, PR+PW+EV, INT, 0, 0, 100, false);


///////////////////////////////

struct RGB_LED : Service::LightBulb {          // RGB LED (Command Cathode)

  Characteristic::On power{0,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{0,true};
  Characteristic::Brightness V{100,true};

  Characteristic::FavoriteHue fH{120,true};           // use custom characteristics to store favorite settings for RGB button
  Characteristic::FavoriteSaturation fS{100,true};
  Characteristic::FavoriteBrightness fV{100,true};
  
  LedPin *redPin, *greenPin, *bluePin;

  float favoriteH=240;
  float favoriteS=100;
  float favoriteV=50;
    
  RGB_LED(int red_pin, int green_pin, int blue_pin, int buttonPin) : Service::LightBulb(){
    
    V.setRange(5,100,1);              // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
    
    redPin=new LedPin(red_pin);        // configures a PWM LED for output to the RED pin
    greenPin=new LedPin(green_pin);    // configures a PWM LED for output to the GREEN pin
    bluePin=new LedPin(blue_pin);      // configures a PWM LED for output to the BLUE pin

    fH.setDescription("Favorite Hue");          // Setting a description, range, and unit allows these Characteristics to be represented generically in the Eve HomeKit App
    fH.setRange(0,360,1);
    fS.setDescription("Favorite Saturation");
    fS.setRange(0,100,1);
    fS.setUnit("percentage");
    fV.setDescription("Favorite Brightness");
    fV.setRange(0,100,1);
    fV.setUnit("percentage");

    new SpanUserCommand('L', "<H S> - set the RGB LED, where H=[0,360] and S=[0,100]", cliSetHSV, this);

    new SpanButton(buttonPin);        // create a control button for the RGB LED

    WEBLOG("Configured PWM LED using RGB pins [%d,%d,%d] with pushbutton on pin %d",red_pin,green_pin,blue_pin,buttonPin);
    
    update();                         // manually call update() to set pixel with restored initial values    
  }

  boolean update() override {

    int p=power.getNewVal();
    
    float h=H.getNewVal<float>();              // range = [0,360]
    float s=S.getNewVal<float>();              // range = [0,100]
    float v=V.getNewVal<float>();              // range = [0,100]

    float r,g,b;
    
    LedPin::HSVtoRGB(h,s/100.0,v/100.0,&r,&g,&b);   // since HomeKit provides S and V in percent, scale down by 100

    redPin->set(r*p*100.0);                         // update the ledPin channels with new values
    greenPin->set(g*p*100.0);    
    bluePin->set(b*p*100.0);

    if(power.updated())
      WEBLOG("HomeKit set PWM LED %s",p?"ON":"OFF");
      
    return(true);                              // return true 
  }

  void button(int pin, int pressType) override {

    switch(pressType){

      case SpanButton::SINGLE:                 // toggle power on/off
        power.setVal(1-power.getVal());
        WEBLOG("Single button press set PWM LED %s",power.getVal()?"ON":"OFF");
      break;

      case SpanButton::DOUBLE:                 // set LED to favorite HSV
        H.setVal(fH.getVal<float>());
        S.setVal(fS.getVal<float>());
        V.setVal(fV.getVal());
        WEBLOG("Double button press set PWM LED to Favorite HSV Color of [%g,%g,%d]",H.getVal<float>(),S.getVal<float>(),V.getVal());
      break;

      case SpanButton::LONG:                   // set favorite HSV to current HSV
        fH.setVal(H.getVal<float>());
        fS.setVal(S.getVal<float>());
        fV.setVal(V.getVal());
        redPin->set(0);                        // turn off LED for 50ms to indicate new favorite has been saved
        greenPin->set(0);
        bluePin->set(0);
        delay(50);
        WEBLOG("Long button press set PWM LED Favorite HSV Color to [%g,%g,%d]",fH.getVal<float>(),fS.getVal<float>(),fV.getVal());
      break;
      
    }
    
    update();  // call to update LED
  }

  static void cliSetHSV(const char *buf, void *arg){
    float h,s;
    char c,dummy;
    RGB_LED *rgbLED=(RGB_LED *)arg;
   
    if(sscanf(buf,"%c %f %f %[^ \t]\n",&c,&h,&s,&dummy)!=3){
      Serial.printf("usage: %c <H S>, where H=[0,360] and S=[0,100]\n\n",c);
      return;
    }
  
    WEBLOG("Setting PWM LED to H=%.1f, S=%.1f",h,s);
    rgbLED->power.setVal(1);
    rgbLED->H.setVal(h);
    rgbLED->S.setVal(s);
    rgbLED->update();      
  }  
  
};
      
///////////////////////////////

struct NeoPixel : Service::LightBulb {      // NeoPixel RGB

  Characteristic::On power{0,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{0,true};
  Characteristic::Brightness V{100,true};

  Characteristic::FavoriteHue fH{120,true};           // use custom characteristics to store favorite settings for RGB button
  Characteristic::FavoriteSaturation fS{100,true};
  Characteristic::FavoriteBrightness fV{100,true};                                
  
  Pixel *pixel;

  float favoriteH=240;
  float favoriteS=100;
  float favoriteV=50;
  
  NeoPixel(int pin, int touchPin) : Service::LightBulb(){

    V.setRange(5,100,1);                      // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
    pixel=new Pixel(pin);                     // creates pixel LED on specified pin using default timing parameters suitable for most SK68xx LEDs

    fH.setDescription("Favorite Hue");          // Setting a description, range, and unit allows these Characteristics to be represented generically in the Eve HomeKit App
    fH.setRange(0,360,1);
    fS.setDescription("Favorite Saturation");
    fS.setRange(0,100,1);
    fS.setUnit("percentage");
    fV.setDescription("Favorite Brightness");
    fV.setRange(0,100,1);
    fV.setUnit("percentage");
    
    new SpanUserCommand('P', "<H S> - set the Pixel LED, where H=[0,360] and S=[0,100]", cliSetHSV, this);

#if SOC_TOUCH_SENSOR_NUM > 0
    new SpanButton(touchPin, PushButton::TOUCH);        // create a control button for the RGB LED
#endif
    
    WEBLOG("Configured NeoPixel LED on pin %d with touch sensor on pin %d",pin,touchPin);

    update();                                 // manually call update() to set pixel with restored initial values    
  }

  boolean update() override {

    int p=power.getNewVal();
    
    float h=H.getNewVal<float>();                // range = [0,360]
    float s=S.getNewVal<float>();                // range = [0,100]
    float v=V.getNewVal<float>();                // range = [0,100]

    pixel->set(Pixel::HSV(h,s,v*p));
          
    if(power.updated())
      WEBLOG("HomeKit set NeoPixel LED %s",p?"ON":"OFF");

    return(true);  
  }

  void button(int pin, int pressType) override {

    switch(pressType){

      case SpanButton::SINGLE:                 // toggle power on/off
        power.setVal(1-power.getVal());
        WEBLOG("Single button press set NeoPixel LED %s",power.getVal()?"ON":"OFF");
      break;

      case SpanButton::DOUBLE:                 // set LED to favorite HSV
        H.setVal(fH.getVal<float>());
        S.setVal(fS.getVal<float>());
        V.setVal(fV.getVal());
        WEBLOG("Double button press set NeoPixel LED to Favorite HSV Color of [%g,%g,%d]",H.getVal<float>(),S.getVal<float>(),V.getVal());
      break;

      case SpanButton::LONG:                   // set favorite HSV to current HSV
        fH.setVal(H.getVal<float>());
        fS.setVal(S.getVal<float>());
        fV.setVal(V.getVal());
        pixel->set(Pixel::RGB(0,0,0));         // turn off NeoPixel for 50ms to indicate new favorite has been saved
        delay(50);
        WEBLOG("Long button press set NeoPixel LED Favorite HSV Color to [%g,%g,%d]",fH.getVal<float>(),fS.getVal<float>(),fV.getVal());
      break;
      
    }
    
    update();  // call to update LED
  }

  static void cliSetHSV(const char *buf, void *arg){
    float h,s;
    char c,dummy;
    NeoPixel *neoPixel=(NeoPixel *)arg;
   
    if(sscanf(buf,"%c %f %f %[^ \t]\n",&c,&h,&s,&dummy)!=3){
      Serial.printf("usage: %c <H S>, where H=[0,360] and S=[0,100]\n\n",c);
      return;
    }
  
    WEBLOG("Setting NeoPixel to H=%.1f, S=%.1f",h,s);
    neoPixel->power.setVal(1);
    neoPixel->H.setVal(h);
    neoPixel->S.setVal(s);
    neoPixel->update();      
  }   

};

///////////////////////////////

struct TempSensor : Service::TemperatureSensor {     // A standalone Temperature sensor

  Characteristic::CurrentTemperature temp{-40};
  Characteristic::StatusActive status{0};
  
  int addr;                                          // I2C address of temperature sensor
  uint32_t timer=0;                                  // keep track of time since last update
  
  TempSensor(int addr) : Service::TemperatureSensor(){       // constructor() method

    this->addr=addr;                      // I2C address of temperature sensor
    temp.setRange(-50,100);

    Wire.setPins(SDA,SCL);                // set I2C pins
    Wire.begin();                         // start I2C in Controller Mode
    
    WEBLOG("Configured Temp Sensor using I2C SDA=%d and SCL=%d pins",SDA,SCL);

    readSensor();                         // initial read of Sensor
        
  } // end constructor

  void loop(){

    char c[64];

    if(millis()-timer>5000){                // only sample every 5 seconds
      timer=millis();
      readSensor();      
    }
    
  } // loop

  void readSensor(){

    char c[64];
    uint8_t oldStatus=status.getVal();
    
    Wire.beginTransmission(addr);         // setup transmission
    Wire.write(0x0B);                     // ADT7410 Identification Register
    Wire.endTransmission(0);              // transmit and leave in restart mode to allow reading
    Wire.requestFrom(addr,1);             // request read of single byte
    uint8_t id=Wire.read();               // receive a byte

    if(id!=0xCB){                         // problem reading from chip
      if(oldStatus){                      // oldStatus was okay
        status.setVal(0);                   // set status to inactive, and
        temp.setVal(-40);                   // set temperature to -40
        
        sprintf(c,"ADT7410-%02X Temperature Sensor is INACTIVE\n",addr);
        LOG1(c);
      }
      return;
    }

    if(!oldStatus){                       // oldStatus was not okay
      Wire.beginTransmission(addr);         // setup transmission
      Wire.write(0x03);                     // ADT740 Configuration Register
      Wire.write(0xC0);                     // set 16-bit temperature resolution, 1 sample per second
      Wire.endTransmission();               // transmit      
      status.setVal(1);
      sprintf(c,"ADT7410-%02X Temperature Sensor is ACTIVE\n",addr);
      LOG1(c);
    }

    Wire.beginTransmission(addr);         // setup transmission
    Wire.write(0x00);                     // ADT7410 2-byte Temperature
    Wire.endTransmission(0);              // transmit and leave in restart mode to allow reading
    Wire.requestFrom(addr,2);             // request read of two bytes

    double tempC;
    int16_t iTemp;
    
    iTemp=((int16_t)Wire.read()<<8)+Wire.read();    
    tempC=iTemp/128.0;

    if(abs(temp.getVal<double>()-tempC)>0.50){    // only update temperature if change is more than 0.5C     
      temp.setVal(tempC);
      
      sprintf(c,"ADT7410-%02X Temperature Update: %g\n",addr,tempC);
      LOG1(c);
    }
    
  }  // readSensor
  
};
      
///////////////////////////////

void setup() {
  
  Serial.begin(115200);

//  while(1){
//    touch_value_t x=0;
//    for(int i=0;i<20;i++)
//      x+=touchRead(F12);
//    Serial.println(x/20);
//    delay(1000);
//  }

  homeSpan.setControlPin(CONTROL_PIN);
  homeSpan.setStatusPin(STATUS_PIN);
  homeSpan.enableOTA();
  homeSpan.setLogLevel(1);
  homeSpan.setWifiCallback(wifiEstablished);
  homeSpan.enableWebLog(50,"pool.ntp.org","CST6CDT");
  homeSpan.setSketchVersion("2.0");

  homeSpan.setPairCallback([](boolean paired){Serial.printf("\n*** DEVICE HAS BEEN %sPAIRED ***\n\n",paired?"":"UN-");});

  homeSpan.begin(Category::Bridges,"HomeSpan UnitTest" DEVICE_SUFFIX);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Model("HomeSpan Unit Test" DEVICE_SUFFIX);
      new Characteristic::FirmwareRevision(homeSpan.getSketchVersion());

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("NeoPixel LED");
    new NeoPixel(PIXEL_PIN,PIXEL_BUTTON);
 
  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("PWM LED");
    new RGB_LED(LED_RED_PIN,LED_BLUE_PIN,LED_GREEN_PIN,LED_BUTTON);   

  new SpanAccessory();   
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("Temp Sensor");
      new Characteristic::SerialNumber("ADT7410");    
      new Characteristic::Model("Adafruit I2C Temp Sensor");
    new TempSensor(0x48);
      
  homeSpan.autoPoll();       // start homeSpan.poll() in background
      
}

///////////////////////////////

void loop() {
}

///////////////////////////////

void wifiEstablished(){

  #define printPin(X)   Serial.printf("%-15s = %4s (GPIO %d)\n",#X,STRINGIFY(X),X);

  Serial.printf("\nHOMESPAN UNIT TEST PINS:\n\n");
  printPin(CONTROL_PIN);
  printPin(STATUS_PIN);
  printPin(LED_RED_PIN);
  printPin(LED_BLUE_PIN);
  printPin(LED_GREEN_PIN);
  printPin(LED_BUTTON);
  printPin(PIXEL_PIN);
  printPin(PIXEL_BUTTON);
  Serial.printf("\n\n");
}
