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

#if defined(CONFIG_IDF_TARGET_ESP32C3)

  #define CONTROL_PIN     1
  #define STATUS_PIN      10
  #define PIXEL_PIN       8
  #define LED_RED_PIN     7
  #define LED_GREEN_PIN   5
  #define LED_BLUE_PIN    0
  #define LED_BUTTON      9
  #define CHIP            "32C3"
  
#elif defined(CONFIG_IDF_TARGET_ESP32S2)

  #define CONTROL_PIN     14
  #define STATUS_PIN      15
  #define PIXEL_PIN       18
  #define LED_RED_PIN     35
  #define LED_GREEN_PIN   33
  #define LED_BLUE_PIN    13
  #define LED_BUTTON      38
  #define CHIP            "32S2"
  
#elif defined(CONFIG_IDF_TARGET_ESP32)

  #error "Unit Test Not Avaiable for ESP32"
  
#endif
 
#include "HomeSpan.h"
#include "extras/Pixel.h"
#include "extras/PwmPin.h"
#include "DEV_Identify.h"

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
    
    new SpanButton(buttonPin);        // create a control button for the RGB LED
    
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
      
    return(true);                              // return true
  
  }

  void button(int pin, int pressType) override {

    switch(pressType){

      case SpanButton::SINGLE:                 // toggle power on/off
        power.setVal(1-power.getVal());
      break;

      case SpanButton::DOUBLE:                 // set LED to favorite HSV
        H.setVal(fH.getVal<float>());
        S.setVal(fS.getVal<float>());
        V.setVal(fV.getVal());
      break;

      case SpanButton::LONG:                   // set favorite HSV to current HSV
        fH.setVal(H.getVal<float>());
        fS.setVal(S.getVal<float>());
        fV.setVal(V.getVal());
        redPin->set(0);                        // turn off LED for 50ms to indicate new favorite has been saved
        greenPin->set(0);
        bluePin->set(0);
        delay(50);
      break;
      
    }
    
    update();  // call to update LED
  }
  
};
      
///////////////////////////////

struct Pixel_Light : Service::LightBulb {      // Addressable RGB Pixel
 
  Characteristic::On power{0,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{0,true};
  Characteristic::Brightness V{100,true};
  Pixel *pixel;                                
  
  Pixel_Light(int pin) : Service::LightBulb(){

    V.setRange(5,100,1);                      // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
    pixel=new Pixel(pin);                     // creates pixel LED on specified pin using default timing parameters suitable for most SK68xx LEDs
    update();                                 // manually call update() to set pixel with restored initial values    
  }

  boolean update() override {

    int p=power.getNewVal();
    
    float h=H.getNewVal<float>();                // range = [0,360]
    float s=S.getNewVal<float>();                // range = [0,100]
    float v=V.getNewVal<float>();                // range = [0,100]

    pixel->setHSV(h*p, s*p/100.0, v*p/100.0);    // must scale down S and V from [0,100] to [0.0,1.0]
          
    return(true);  
  }

};

///////////////////////////////

Pixel_Light *pixelLight;
RGB_LED *rgbLED;

void setup() {
  
  Serial.begin(115200);

  homeSpan.setControlPin(CONTROL_PIN);
  homeSpan.setStatusPin(STATUS_PIN);
  homeSpan.enableOTA();

  new SpanUserCommand('P', "<H S> - set the Pixel LED, where H=[0,360] and S=[0,100]", setHSV);
  new SpanUserCommand('L', "<H S> - set the RGB LED, where H=[0,360] and S=[0,100]", setHSV);
 
  homeSpan.begin(Category::Bridges,"HomeSpan Unit Test " CHIP);

  new SpanAccessory();
    new DEV_Identify("HomeSpan Unit Test " CHIP,"HomeSpan ",CHIP,"HS Bridge","1.0",3);
    new Service::HAPProtocolInformation();
      new Characteristic::Version("1.1.0");

  new SpanAccessory();
    new DEV_Identify("Pixel LED","HomeSpan",CHIP,"SK68XX","1.0",3);
    pixelLight=new Pixel_Light(PIXEL_PIN);
 
  new SpanAccessory();
    new DEV_Identify("PWM LED","HomeSpan",CHIP,"RGB LED","1.0",3);
    rgbLED=new RGB_LED(LED_RED_PIN,LED_BLUE_PIN,LED_GREEN_PIN,LED_BUTTON);

      
}

///////////////////////////////

void loop() {
  homeSpan.poll();
}

///////////////////////////////

void setHSV(const char *buf){
  float h,s,r,g,b;
  char c,dummy;
 
  if(sscanf(buf,"%c %f %f %[^ \t]\n",&c,&h,&s,&dummy)!=3){
    Serial.printf("usage: %c <H S V>, where H=[0,360] and S=[0,100]\n\n",c);
    return;
  }

  switch(c){

    case 'P':
      Serial.printf("Setting Pixel Light to H=%.1f, S=%.1f\n\n",h,s);
      pixelLight->power.setVal(1);
      pixelLight->H.setVal(h);
      pixelLight->S.setVal(s);
      pixelLight->update();      
    break;

    case 'L':
      Serial.printf("Setting RGB LED Light to H=%.1f, S=%.1f \n\n",h,s);
      rgbLED->power.setVal(1);
      rgbLED->H.setVal(h);
      rgbLED->S.setVal(s);
      rgbLED->update();
    break;
  }
    
}
