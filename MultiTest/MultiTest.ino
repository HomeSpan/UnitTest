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

#ifndef HS_FEATHER_PINS
#error "Can't compile - Feather Pins NOT defined"
#endif

#include <Wire.h>

#include "HomeSpan.h"
#include "SpanRollback.h"

#define CONTROL_PIN     F25
#define STATUS_PIN      F26
#define LED_RED_PIN     F33
#define LED_GREEN_PIN   F32
#define LED_BLUE_PIN    F14
#define LED_BUTTON      F4
#define PIXEL_PIN       F27
#define CONTACT_SWITCH  F21

#if SOC_TOUCH_SENSOR_NUM > 0
  #define PIXEL_BUTTON  F12
#else
  #define PIXEL_BUTTON  -1
#endif

// Create Custom Characteristics to save a "favorite" HSV color

CUSTOM_CHAR(FavoriteHue, 00000001-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 360, false);
CUSTOM_CHAR(FavoriteSaturation, 00000002-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 100, false);
CUSTOM_CHAR(FavoriteBrightness, 00000003-0001-0001-0001-46637266EA00, PR+PW+EV, INT, 0, 0, 100, false);

CUSTOM_CHAR_STRING(FreeStringRW,00000020-0001-0001-0001-46637266EA00,PR+PW,"");
CUSTOM_CHAR_STRING(FreeStringRV,00000050-0001-0001-0001-46637266EA00,PR+EV,"");

boolean bootControlButton;
boolean bootLedButton;
boolean bootToggleSwitch;

///////////////////////////////

struct RGB_LED : Service::LightBulb {          // RGB LED (Common Cathode)

  Characteristic::On power{0,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{0,true};
  Characteristic::Brightness V{100,true};

  Characteristic::FavoriteHue fH{120,true};           // use custom characteristics to store favorite settings for RGB button
  Characteristic::FavoriteSaturation fS{100,true};
  Characteristic::FavoriteBrightness fV{100,true};
  
  SpanCharacteristic *stringRW;
  SpanCharacteristic *stringRV;
  
  LedPin *redPin, *greenPin, *bluePin;

  float favoriteH=240;
  float favoriteS=100;
  float favoriteV=50;
    
  RGB_LED(int red_pin, int green_pin, int blue_pin, int buttonPin) : Service::LightBulb(){

    new Characteristic::ConfiguredName("PWM LED",true);
    stringRW=(new Characteristic::FreeStringRW("String RW",true))->setDescription("RW String");
    stringRV=(new Characteristic::FreeStringRV("String RV",true))->setDescription("RV String");
    
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

    redPin->fade(r*p*100.0,1000,LedPin::PROPORTIONAL);                         // update the ledPin channels with new values
    greenPin->fade(g*p*100.0,1000,LedPin::PROPORTIONAL);    
    bluePin->fade(b*p*100.0,1000,LedPin::PROPORTIONAL);
    
    if(power.updated())
      WEBLOG("HomeKit set PWM LED %s",p?"ON":"OFF");

    if(stringRW->updated())
      stringRV->setString(stringRW->getNewString());
      
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

  Characteristic::ConfiguredName name{"NeoPixel LED",true};
  
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
    new SpanButton(touchPin, SpanButton::TRIGGER_ON_TOUCH);        // create a control button for the Pixel LED
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
  
  TempSensor(int addr) : Service::TemperatureSensor(){       // constructor() method

    new Characteristic::ConfiguredName("Temp Sensor",true);

    this->addr=addr;                      // I2C address of temperature sensor
    temp.setRange(-50,100);

    Wire.setPins(SDA,SCL);                // set I2C pins
    Wire.begin();                         // start I2C in Controller Mode   
    Wire.beginTransmission(addr);         // setup transmission
    Wire.write(0x0B);                     // ADT7410 Identification Register
    Wire.endTransmission(0);              // transmit and leave in restart mode to allow reading
    Wire.requestFrom(addr,1);             // request read of single byte

    if(Wire.read()==0xCB){                // correct ID of temperature sensor read      
      Wire.beginTransmission(addr);       // setup transmission
      Wire.write(0x03);                   // ADT740 Configuration Register
      Wire.write(0xC0);                   // set 16-bit temperature resolution, 1 sample per second
      Wire.endTransmission();             // transmit      
      status.setVal(1);                   // set status to ACTIVE
    }

    WEBLOG("ADT7410-%02X Temperature Sensor %sfound\n",addr,status.getVal()?"":"NOT ");
        
  } // end constructor

  void loop(){

    if(status.getVal() && status.timeVal()>5000)        // if temp sensor ACTIVE, sample every 5 seconds
      readSensor();      
    
  } // loop

  void readSensor(){

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
     
      LOG0("ADT7410-%02X Temperature Update: %g\n",addr,tempC);
    }
    
  }  // readSensor
  
};

///////////////////////////////

struct ContactSwitch : Service::ContactSensor {

  SpanCharacteristic *sensorState;
  SpanToggle *toggleSwitch;
  
  ContactSwitch(int togglePin) : Service::ContactSensor(){      // constructor

    new Characteristic::ConfiguredName("Contact Switch",true);

    toggleSwitch=new SpanToggle(togglePin,SpanToggle::TRIGGER_ON_HIGH);                                // create toggle switch connected to VCC    
    sensorState=new Characteristic::ContactSensorState(toggleSwitch->position()==SpanToggle::OPEN);    // instantiate contact sensor state
    
    WEBLOG("Configured Contact Switch on pin %d",togglePin);    
  }

  void button(int pin, int position) override {      
    
      WEBLOG("Contact Switch %s",position==SpanToggle::CLOSED?"CLOSED":"OPEN");
      sensorState->setVal(position==SpanToggle::OPEN?1:0);   
  }  
  
};

NeoPixel *savedNeoPixel;

///////////////////////////////

void setup() {
  
  Serial.begin(115200);

  homeSpan.setControlPin(CONTROL_PIN)
          .setStatusPin(STATUS_PIN)
          .setLogLevel(2)
          .setConnectionCallback(connectionEstablished)
          .setSketchVersion("2025.10.24")
          .enableWebLog(50,"pool.ntp.org","CST6CDT").setWebLogFavicon()
          .setPairCallback([](boolean paired){Serial.printf("\n*** DEVICE HAS BEEN %sPAIRED ***\n\n",paired?"":"UN-");})
          .setStatusCallback([](HS_STATUS status){Serial.printf("\n*** HOMESPAN STATUS: %s\n\n",homeSpan.statusString(status));})
          .setWebLogCSS(".bod1 {background-color:lightyellow;}"
                        ".bod1 h2 {color:blue;}"
                        ".tab1 {background-color:lightgreen;}"
                        ".tab2 {background-color:lightblue;} .tab2 th {color:red;} .tab2 td {color:darkblue; text-align:center;}"
                       )
          .setPairingCode("34456777")
          .setConnectionTimes(5,60,3)
          .enableWiFiRescan(1,2)
          .addBssidName("34:98:B5:DB:3E:C0","Great Room - 2.4 GHz")
          .addBssidName("3A:98:B5:DB:53:5E","Upstairs Hallway - 2.4 GHz")
          .addBssidName("3A:98:B5:EF:BF:69","Kitchen - 2.4 GHz")
          .addBssidName("3A:98:B5:DB:54:86","Basement - 2.4 GHz")
          .addBssidName("34:98:B5:DB:3E:C1","Great Room - 5.0 GHz")
          .addBssidName("3A:98:B5:DB:53:5F","Upstairs Hallway - 5.0 GHz")
          .addBssidName("3A:98:B5:EF:BF:6A","Kitchen - 5.0 GHz")
          .addBssidName("3A:98:B5:DB:54:87","Basement - 5.0 GHz")
          .setPollingCallback([](){homeSpan.markSketchOK();})
          .enableWatchdog(15)
          .setCompileTime();

  homeSpan.enableOTA("unit-test");

  WiFi.enableIPv6();
  ETH.enableIPv6();

  #if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)) && SOC_WIFI_SUPPORT_5G
    WiFi.STA.begin();
    WiFi.setBandMode(WIFI_BAND_MODE_5G_ONLY);
    homeSpan.setConnectionTimes(30,60,5);
  #endif

//  homeSpan.useEthernet();

// C3 WARNING: pin 16 is used for Serial RX. If ETH is selected, chip will not allow input from Serial Monitor

  ETH.begin(ETH_PHY_W5500, 1, F16, -1, -1, SPI2_HOST, SCK, MISO, MOSI);

//  ETH.begin(ETH_PHY_RTL8201, 0, 16, 17, -1, ETH_CLOCK_GPIO0_IN);

  new SpanUserCommand('T', " - print the time",[](const char *buf){
    struct tm cTime;
    getLocalTime(&cTime);
    Serial.printf("Current Time = %02d/%02d/%04d  %02d:%02d:%02d\n",cTime.tm_mon+1,cTime.tm_mday,cTime.tm_year+1900,cTime.tm_hour,cTime.tm_min,cTime.tm_sec);
  });

  homeSpan.setWebLogCallback([](String &r){r="<tr><td>Free DRAM:</td><td>" + String(esp_get_free_internal_heap_size()) + " bytes</td></tr>\n" +
    "</table><p><a href=\"https://github.com/HomeSpan/HomeSpan\">Click Here to Access HomeSpan Repo</a></p>";});

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
    savedNeoPixel = new NeoPixel(PIXEL_PIN,PIXEL_BUTTON); 

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("PWM LED");
    new RGB_LED(LED_RED_PIN,LED_GREEN_PIN,LED_BLUE_PIN,LED_BUTTON);   

  new SpanAccessory();   
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name("Temp Sensor");
      new Characteristic::SerialNumber("ADT7410");    
      new Characteristic::Model("Adafruit I2C Temp Sensor");
    new TempSensor(0x48);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("Contact Switch");
    new ContactSwitch(CONTACT_SWITCH);

  new SpanUserCommand('D',"- disable HomeSpan watchdog",[](const char *buf){homeSpan.disableWatchdog();});
  new SpanUserCommand('T',"<nSec> - execute a time delay of nSec seconds",[](const char *buf){delay(atoi(buf+1)*1000);});
  new SpanUserCommand('E',"- call ETH",[](const char *buf){ETH.begin(ETH_PHY_W5500, 1, F16, -1, -1, SPI2_HOST, SCK, MISO, MOSI);});
  new SpanUserCommand('N',"<name> - rename NeoPixel",[](const char *buf){savedNeoPixel->name.setString(buf+1);});
  new SpanUserCommand('n',"- show network info",[](const char *buf){Serial.printf("\n\n");ETH.printTo(Serial);Serial.printf("\n\n");WiFi.STA.printTo(Serial);});
  
  new SpanUserCommand('R',"- show NeoPixel name bytes",[](const char *buf){
    char *p=savedNeoPixel->name.getString();
    for(int i=0;i<strlen(p);i++)
      Serial.printf(" %02X",p[i]);
    Serial.printf("\n");
  }); 

  bootControlButton=!digitalRead(CONTROL_PIN);
  bootLedButton=!digitalRead(LED_BUTTON);
  bootToggleSwitch=digitalRead(CONTACT_SWITCH);

  if(bootLedButton){
    if(bootToggleSwitch){
      Serial.printf("\n\n*** BOOT TEST: FORCING PANIC IN 2 SECONDS...\n");
      delay(2000);
      int *p=NULL;
      *p=0;
    } else {
      Serial.printf("\n\n*** BOOT TEST: ENTERING INFINITE LOOP...\n");
      while(1);
    }
  }

  if(bootControlButton){
    if(bootToggleSwitch)
      homeSpan.autoPoll(8192,1,1);
    else
      homeSpan.autoPoll();  
  }
      
}

///////////////////////////////

void loop() {

  if(!bootControlButton)
    homeSpan.poll();

  if(savedNeoPixel->power.timeVal()>60000){
    homeSpanPAUSE;
    LOG0("*** Changing state of NeoPixel\n");
    savedNeoPixel->power.setVal(1-savedNeoPixel->power.getVal());
    savedNeoPixel->update();
  }

}

///////////////////////////////

void connectionEstablished(int nConnects){

  if(nConnects>1)
    return;

  #define printPin(X)   Serial.printf("%-15s = GPIO %d\n",#X,X);
 
  Serial.printf("\nHOMESPAN UNIT TEST PINS:\n\n");
  printPin(CONTROL_PIN);
  printPin(STATUS_PIN);
  printPin(LED_RED_PIN);
  printPin(LED_BLUE_PIN);
  printPin(LED_GREEN_PIN);
  printPin(LED_BUTTON);
  printPin(PIXEL_PIN);
  printPin(PIXEL_BUTTON);
  printPin(CONTACT_SWITCH);
  Serial.printf("\n\n");
}

///////////////////////////////
