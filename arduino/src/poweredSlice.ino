/*
Wisdom Exchange code, January 2014 (TOGhibition in the Exchange)
 
 Sleep until button is pressed, then roll one wisdom's worth of paper,
 then slice, then sleep.
 
 Slice watchdog code commented out; think it caused more problems than it solved.
 Consider reinstating it for battery power.

 Slicer calibrates on powerup, then uses that calibration value to know when to PWM slowly home.
 For battery power, add constant checking of this value to handle battery voltage drop.
 */

#include <avr/sleep.h>
#include <avr/power.h>

//int watchdogMinor = 0;
//int watchdogMajor = 0;

long slicerRunTime;

void setup() {                
  // initialize the digital pin as an output.
  Serial.begin(9600);

  pinMode(8, OUTPUT); //1,2EN and 3,4EN
  pinMode(3, OUTPUT); //1A
  pinMode(4, OUTPUT); //2A


  pinMode(6, OUTPUT); //3A
  pinMode(7, OUTPUT); //4A

  pinMode(2, INPUT); //BUTTON, 1=open, 0=pressed
  pinMode(9, OUTPUT); //SLICE 
  pinMode(10, INPUT); //LIMIT, 0=at limit switch, 1=in slice


  pinMode(13, OUTPUT); //Arduino LED

  digitalWrite(3, LOW);  // 1A
  digitalWrite(4, LOW);  // 2A
  digitalWrite(6, LOW);  // 3A
  digitalWrite(7, LOW);  // 4A  
  digitalWrite(9, LOW);  // slice

  digitalWrite(8, LOW);   //disable 1A,2A,3A,4A on H-bridge chip

  //calibrate and set slicer
  slicerRunTime = getSlicerRunTime();
  slice();

  enterSleep();


}

void enterSleep(void)
{

  /* Setup pin2 as an interrupt and attach handler. */
  attachInterrupt(0, pin2Interrupt, LOW);
  delay(100);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //most power saving

  sleep_enable();

  sleep_mode();

  /* The program will continue from here. */

  /* First thing to do is disable sleep. */
  sleep_disable(); 

  runWisdom();

  //in case they are still holding the button, wait for them to stop (otherwise it crashes :S)
  while(digitalRead(2) == 0)
  {
    //wait
  } 

  enterSleep();
}

void pin2Interrupt(void)
{
  /* This will bring us back from sleep. */

  /* We detach the interrupt to stop it from 
   * continuously firing while the interrupt pin
   * is low.
   */
  detachInterrupt(0);
}

void  runWisdom() {
  int distance = 922; //about 15.4cm on USB mains. Note: it's not the writing area width that 
                      //matters but the length of the paper inside the machine.
  //int distance = 50; //for testing, 8mm on mains

  drive(distance);
  slice();

}



void drive(int distance){
  //1, 1&3, 3, 3&2, 2, 2&4, 4, 4&1

  digitalWrite(8, HIGH);   //enable 1,2,3,4

  int speed = 2;
  for(int i=0; i<distance; i++){

    //(4)&1
    digitalWrite(3, HIGH);  // 1A on
    delay(speed);

    //1
    digitalWrite(7, LOW);  // 4A off
    delay(speed);

    //1&3
    digitalWrite(6, HIGH);  // 3A on
    delay(speed);

    //3
    digitalWrite(3, LOW);  // 1A off
    delay(speed);

    //3&2
    digitalWrite(4, HIGH);  // 2A on
    delay(speed);

    //2
    digitalWrite(6, LOW);  // 3A off
    delay(speed);

    //2&4
    digitalWrite(7, HIGH);  // 4A on
    delay(speed);

    //4
    digitalWrite(4, LOW);  // 2A off
    delay(speed);
  }

  digitalWrite(8, LOW);   //disable 1,2,3,4
}

void slice() {
  digitalWrite(13, HIGH);  // led on
  // if we're at the limit already, drive until off and then some
  // if we started away from the limit, drive until we hit it and them some

  //the drive until one again
  //then a bit for luck

  //so

  //startWatchdog(); //count down while slicing so that it will stop if it gets stuck


  if(digitalRead(10) == 1) // ie we started with the slicer somewhere in the slice
  {
    while(digitalRead(10) == 1){ // ie open
      digitalWrite(9, HIGH);  // slice until closed
      //kickWatchdog();
    }
    // it's now closed
    delay(10); //debounce
  }

  //leaving limit switch
  digitalWrite(9, HIGH);  // slice
  while(digitalRead(10) == 0){ // ie closed
    //kickWatchdog();
  }

  long startOutside = millis();
  //  it's now open
  delay(10); //debounce

  //run fast for 80% of time required
  while(millis() < startOutside+slicerRunTime){ // ie open
    //kickWatchdog();
    // keep slicing
  }

  //then PWM it slowly back to the switch
  analogWrite(9, 128);
  while(digitalRead(10) == 1){ // ie open
  }
  
  // it's now closed
  digitalWrite(9, LOW);  // stop slicing immediately
  delay(10); //debounce AFTER stopping
  digitalWrite(13, LOW);  // led off

  //Serial.println(watchdogMajor);
  //Serial.println(watchdogMinor);
}

long getSlicerRunTime() {
  digitalWrite(13, HIGH);  // led on
  // if we're at the limit already, drive until off and then some
  // if we started away from the limit, drive until we hit it and them some

  //the drive until one again
  //then a bit for luck

  //so

  //startWatchdog(); //count down while slicing so that it will stop if it gets stuck


  if(digitalRead(10) == 1) // ie we started with the slicer somewhere in the slice
  {
    while(digitalRead(10) == 1){ // ie open
      digitalWrite(9, HIGH);  // slice until closed
      //kickWatchdog();
    }
    // it's now closed
    delay(10); //debounce
  }

  //leaving limit switch
  digitalWrite(9, HIGH);  // slice
  while(digitalRead(10) == 0){ // ie closed
    //kickWatchdog();
  }

  long startOutside = millis();
  //  it's now open
  delay(10); //debounce

  while(digitalRead(10) == 1){ // ie open
    //kickWatchdog();
    // keep slicing
  }
  long timeOutside = millis() - startOutside;

  // it's now closed
  digitalWrite(9, LOW);  // stop slicing immediately
  delay(10); //debounce AFTER stopping
  digitalWrite(13, LOW);  // led off


  return timeOutside * 0.7; //70% of time required

}






/*
void startWatchdog(){
 watchdogMinor = 32767;
 watchdogMajor = 32767;
 }
 
 
 void kickWatchdog()
 {
 watchdogMinor--;
 if (watchdogMinor == 0)
 {
 watchdogMinor = 32767;
 if (watchdogMajor > 0) watchdogMajor--;
 }
 }
 */

boolean watchdogOk()
{
  //powered by USB: regular slice takes 32765, double slice takes 32763. Batteries will be ~2x slower, hence this value
  //return watchdogMajor > 32758;
  return true;
}

void loop() {
  //nothing to do (using sleep system to loop instead)
}



