/*
   Wisdom Exchange code, Burning Man 2018

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
int cyanLevel;

const int ENABLE = 8;     // enable H-bridges that drive roller stepper motor
const int ROLLER_1A = 3;  // roller stepper motor
const int ROLLER_2A = 4;  // "
const int ROLLER_3A = 6;  // "
const int ROLLER_4A = 7;  // "
const int BUTTON = 2;     // brass button on top. 1=open, 0=pressed
const int SLICER = 9;     // DC motor that drives paper slicer
const int LIMIT = 10;     // limit switch on slicer. 0=slicer at limit switch, 1=somewhere else
const int DEBUG_LED = 13; // LED on Arduino
const int CYAN_LEDS = 11; // decorative LEDs in round panel

const int OFF = LOW;
const int ON = HIGH;
const int PRESSED = 0; // button state
const int CLOSED = 0; // limit switch closed by slicer
const int OPEN = 1; // limit switch open

void setup() {                
    // Serial.begin(9600);


    pinMode(ENABLE, OUTPUT);
    pinMode(ROLLER_1A, OUTPUT);
    pinMode(ROLLER_2A, OUTPUT);
    pinMode(ROLLER_3A, OUTPUT);
    pinMode(ROLLER_4A, OUTPUT);
    pinMode(BUTTON, INPUT);
    pinMode(SLICER, OUTPUT);
    pinMode(LIMIT, INPUT);
    pinMode(DEBUG_LED, OUTPUT);
    pinMode(CYAN_LEDS, OUTPUT);

    digitalWrite(ROLLER_1A, OFF);
    digitalWrite(ROLLER_2A, OFF);
    digitalWrite(ROLLER_3A, OFF);
    digitalWrite(ROLLER_4A, OFF);
    digitalWrite(SLICER, OFF);
    digitalWrite(CYAN_LEDS, OFF);

    digitalWrite(ENABLE, OFF); //disable 1A,2A,3A,4A on H-bridge chip

    //calibrate and set slicer
    slicerRunTime = getSlicerRunTime();
    // Serial.println(slicerRunTime);
    slice();

    enterSleep();


}

void enterSleep(void)
{

    while(true) {

        //in case they are still holding the button, wait for them to let go
        while(digitalRead(BUTTON) == PRESSED)
        {
            delay(1);
        } 

        /* Set up button press as an interrupt and attach handler. */
        attachInterrupt(0, buttonInterrupt, PRESSED);
        delay(100);

        // Go to sleep
        set_sleep_mode(SLEEP_MODE_PWR_DOWN); //most power saving
        sleep_enable();
        sleep_mode();

        // zzzz

        /* The program will continue from here. */
        /* First thing to do is disable sleep. */
        sleep_disable(); 

        runWisdom();


    }
    // enterSleep();
}

void buttonInterrupt(void)
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
    // matters but the length of the paper inside the machine.
    // int distance = 50; //for testing, 8mm on mains

    fadeUp(1500);
    drive(distance);
    slice();
    fadeDown(3000);
}

void fadeUp(int time){
    // fade in lights from zero
    for(int i=0; i<time; i++){
        analogWrite(CYAN_LEDS, int(float(i) / float(time) * 255.0));
        delay(1);
    }
    digitalWrite(CYAN_LEDS, ON);
    cyanLevel=255;
}

void fadeDown(int time){
    // fade out lights from current level
    for(int i=time; i>0; i--){
        analogWrite(CYAN_LEDS, int(float(i) / float(time) * cyanLevel));
        delay(1);
    }
    digitalWrite(CYAN_LEDS, OFF);
    cyanLevel=0;
}

void drive(int distance){
    // stepper motor sequence: 1, 1&3, 3, 3&2, 2, 2&4, 4, 4&1

    digitalWrite(ENABLE, ON);

    int cyanDelta = 0;

    int speed = 2;
    for(int i=0; i<distance; i++){


        //(4)&1
        digitalWrite(ROLLER_1A, ON);
        delay(speed);

        //1
        digitalWrite(ROLLER_4A, OFF);
        delay(speed);

        //1&3
        digitalWrite(ROLLER_3A, ON);
        delay(speed);

        //3
        digitalWrite(ROLLER_1A, OFF);
        delay(speed);

        //3&2
        digitalWrite(ROLLER_2A, ON);
        delay(speed);

        //2
        digitalWrite(ROLLER_3A, OFF);
        delay(speed);

        //2&4
        digitalWrite(ROLLER_4A, ON);
        delay(speed);

        //4
        digitalWrite(ROLLER_2A, OFF);
        delay(speed);

        if (i % 8 == 0) {
            cyanDelta = cyanDelta = random(-5, 5);
        }
        int proposedCyan = cyanLevel + cyanDelta;
        if (proposedCyan >= 168 && proposedCyan <= 255)
        {
            cyanLevel = proposedCyan;
        }
        analogWrite(CYAN_LEDS, cyanLevel);
    }
    digitalWrite(ENABLE, OFF);
}

void slice() {
    digitalWrite(DEBUG_LED, ON);
    // if we're at the limit already, drive until off and then some
    // if we started away from the limit, drive until we hit it and them some

    //the drive until one again
    //then a bit for luck

    //so

    //startWatchdog(); //count down while slicing so that it will stop if it gets stuck


    if(digitalRead(LIMIT) == OPEN)
    {
        // we started with the slicer somewhere mid-slice
        while(digitalRead(LIMIT) == OPEN){
            // so slice until closed
            digitalWrite(SLICER, ON);
            //kickWatchdog();
        }
        // it's now closed
        delay(10); //debounce
    }

    //leave limit switch
    digitalWrite(SLICER, ON);
    while(digitalRead(LIMIT) == CLOSED){
        //kickWatchdog();
    }

    long startOutside = millis();
    //  it's now open
    delay(10); //debounce

    //run fast for 80% of time required
    // ie blast through the paper, turn, most of the way back, but then slow down so we don't overshoot
    while(millis() < startOutside+slicerRunTime){
        //kickWatchdog();
        // keep slicing
    }

    //then PWM it slowly back to the switch
    analogWrite(SLICER, 128);
    while(digitalRead(LIMIT) == OPEN){
    }
    long timeOutside = millis() - startOutside;

    // it's now closed
    digitalWrite(SLICER, OFF);  // stop slicing immediately
    delay(10); //debounce AFTER stopping
    digitalWrite(DEBUG_LED, OFF);

    // update slicer run time, so it stays accurate if voltage drops
    slicerRunTime = timeOutside * 0.7; //70% of time required

    //Serial.println(watchdogMajor);
    //Serial.println(watchdogMinor);
}

long getSlicerRunTime() {
    digitalWrite(DEBUG_LED, ON);
    // this is a first guess, will be a bit lower than the actual time, but it's needed to seed
    // slice operations.
    // if we're at the limit already, drive until off and then some
    // if we started away from the limit, drive until we hit it and them some

    //the drive until one again
    //then a bit for luck

    //so

    //startWatchdog(); //count down while slicing so that it will stop if it gets stuck


    if(digitalRead(LIMIT) == OPEN)
    {
        // we started wiht the slicer somewhere mid-slice
        while(digitalRead(LIMIT) == OPEN){
            digitalWrite(SLICER, ON);  // slice until closed
            //kickWatchdog();
        }
        // it's now closed
        delay(10); //debounce
    }

    //leave limit switch
    digitalWrite(SLICER, ON);
    while(digitalRead(LIMIT) == CLOSED){
        //kickWatchdog();
    }

    long startOutside = millis();
    //  it's now open
    delay(10); //debounce

    while(digitalRead(LIMIT) == OPEN){
        //kickWatchdog();
        // keep slicing
    }
    long timeOutside = millis() - startOutside;

    // it's now closed
    digitalWrite(SLICER, OFF);  // stop slicing immediately
    delay(10); //debounce AFTER stopping
    digitalWrite(DEBUG_LED, OFF);


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



