/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2024 Gregg E. Berman
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

#include "HomeSpan.h"

// Create Custom Characteristics to save a "favorite" HSV color

CUSTOM_CHAR(FavoriteHue, 00000001-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 360, false);
CUSTOM_CHAR(FavoriteSaturation, 00000002-0001-0001-0001-46637266EA00, PR+PW+EV, FLOAT, 0, 0, 100, false);
CUSTOM_CHAR(FavoriteBrightness, 00000003-0001-0001-0001-46637266EA00, PR+PW+EV, INT, 0, 0, 100, false);


struct NeoPixel : Service::LightBulb {      // NeoPixel RGB

  Characteristic::On power{1,true};
  Characteristic::Hue H{0,true};
  Characteristic::Saturation S{100,true};
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

    new SpanButton(touchPin, SpanButton::TRIGGER_ON_TOUCH);        // create a control button for the Pixel LED
    
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

void setup() {
 
  Serial.begin(115200);

  homeSpan.setLogLevel(2)
          .setStatusPin(LED_BUILTIN)
          .setControlPin(0);

  homeSpan.begin(Category::Bridges,"Travel S2");
  
   new SpanAccessory();                       // start with Bridge Accessory
    new Service::AccessoryInformation();  
      new Characteristic::Identify(); 

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
      new Characteristic::Name("NeoPixel LED");
    new NeoPixel(PIN_NEOPIXEL,5);
}

//////////////////////////////////////

void loop(){
 
  homeSpan.poll();
  
}
