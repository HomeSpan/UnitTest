/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2023 Gregg E. Berman
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

// The Accessory we will use two stepper motors:

//   * one motor to open/close the window shade, driven by an Adafruit TB6612 driver board (https://www.adafruit.com/product/2448)
//   * one motor to tilt the window shade slats, driven by a SparkFun A3967 driver board (https://www.sparkfun.com/products/12779)

#include "FeatherPins.h"
#include "HomeSpan.h"

////////////////////////////////////

struct DEV_WindowShade : Service::WindowCovering {

  Characteristic::CurrentPosition currentPos{0,true};
  Characteristic::TargetPosition targetPos{0,true};
  Characteristic::CurrentHorizontalTiltAngle currentTilt{0,true};
  Characteristic::TargetHorizontalTiltAngle targetTilt{0,true};
  
  StepperControl *mainMotor;          // motor to open/close shade
  StepperControl *slatMotor;          // motor to tilt shade slats

  DEV_WindowShade(StepperControl *mainMotor, StepperControl *slatMotor) : Service::WindowCovering(){

    this->mainMotor=mainMotor;                          // save pointers to the motors
    this->slatMotor=slatMotor;
                  
    LOG0("Initial Open/Close Position: %d\n",currentPos.getVal());
    LOG0("Initial Slat Position: %d\n",currentTilt.getVal());
    
    mainMotor->setPosition(currentPos.getVal()*20);       // define initial position of main motor
    slatMotor->setPosition(currentTilt.getVal()*11.47);   // define initial position of slat motor
  }

  ///////////
  
  boolean update(){

    if(targetPos.updated()){
      
      // Move motor to absolute position, assuming 400 steps per revolution and 5 revolutions for full open/close travel,
      // for a total of 2000 steps of full travel. Specify that motor should enter the BRAKE state upon reaching to desired position.
      // Must multiply targetPos, which ranges from 0-100, by 20 to scale to number of motor steps needed
    
      mainMotor->moveTo(targetPos.getNewVal()*20,5,StepperControl::BRAKE);
      LOG1("Setting Shade Position=%d\n",targetPos.getNewVal());
    }

    if(targetTilt.updated()){
      
      // Move motor to absolute position, assuming 2064 steps per revolution and 1/2 revolution for full travel of slat tilt in either direction
      // Must multiply targetPos, which ranges from -90 to 90, by 11.47 to scale number of motor steps needed
      // Note this driver board for this motor does not support a "short brake" state
    
      slatMotor->moveTo(targetTilt.getNewVal()*11.47,5,StepperControl::DISABLE);
      LOG1("Setting Shade Position=%d\n",targetTilt.getNewVal());
    }

    return(true);
  }

  ///////////

  void loop(){
    
    if(currentPos.getVal()!=targetPos.getVal() && !mainMotor->stepsRemaining()){
      currentPos.setVal(targetPos.getVal());
      LOG1("Main Motor Stopped at Shade Position=%d\n",currentPos.getVal());
    }

    if(currentTilt.getVal()!=targetTilt.getVal() && !slatMotor->stepsRemaining()){
      currentTilt.setVal(targetTilt.getVal());
      LOG1("Slat Motor Stopped at Shade Tilt=%d\n",currentTilt.getVal());
    } 
            
  }
  
};

////////////////////////////////////

void setup() {

  Serial.begin(115200);

#if defined(ARDUINO_ESP32S3_DEV)
  homeSpan.setStatusPixel(BUILTIN_PIXEL,0,100,20);
#endif

  homeSpan.setLogLevel(2);
  homeSpan.begin(Category::WindowCoverings,"Motorized Shade");

  new SpanAccessory();                                                          
    new Service::AccessoryInformation();
      new Characteristic::Identify(); 
    new DEV_WindowShade( (new Stepper_TB6612(F23,F32,F22,F14,F33,F27))->setAccel(10,20)->setStepType(StepperControl::HALF_STEP),
                         (new Stepper_A3967(F18,F21,F5,F4,F19))->disable() );
}

//////////////////////////////////////

void loop(){
  
  homeSpan.poll();  
}
