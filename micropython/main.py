# MicroPython v1.18

import time
from time import sleep_us, sleep
import machine
from machine import Pin, PWM, lightsleep
import esp32

pSlice = Pin(22, Pin.OUT)
RUN_SLICER = 0
STOP_SLICER = 1
pwmSlice = PWM(pSlice, freq=5000)
SLICER_STOP = 0
SLICER_QUARTER_SPEED = int(65535/4)
SLICER_HALF_SPEED = int(65535/2)
SLICER_THREE_QUARTER_SPEED = int(65535/4*3)
SLICER_FULL_SPEED = 65535
HOMING_TIME_LIMIT_MS = 2000
HOMING_ATTEMPTS_LIMIT = 3

pLED = Pin(2, Pin.OUT)
pwmLED = PWM(pLED, freq=5000)
LED_FULL_ON = 65535
LED_FULL_OFF = 0
LED_FADE_TIME_EXCHANGE_MS = 2000
LED_FADE_TIME_ATTRACT_MS = 1000
ATTRACT_MODE_FLASH_EVERY_MS = 10000
do_exchange = False

pGO = Pin(4, Pin.IN)
GO_PRESSED = 1

pLimit = Pin(23, Pin.IN)
AT_LIMIT = 0  # i.e. slicer out of the way

# Stepper motor: DRV8825
pDIR = Pin(18, Pin.OUT)
DIR_FORWARDS = 1
pSTEP = Pin(19, Pin.OUT)
pEN = Pin(21, Pin.OUT)
STEPPER_ENABLED = 0
STEPPER_DISABLED = 1
usDelay = 50  # number of microseconds per step
MICROSTEPS_PER_STEP = 32  # as set by hardware
STEPS_PER_MM = 24  # by testing
MM_PER_WISDOM = 124  # using new routing for four wisdoms
MICROSTEPS_PER_WISDOM = int(MM_PER_WISDOM * STEPS_PER_MM * MICROSTEPS_PER_STEP)


def slice_paper():
    print("slicing...")
    # ramp up so that current draw doesn't cause LEDs to flicker
    pwmSlice.duty_u16(SLICER_QUARTER_SPEED)
    time.sleep(0.05)
    pwmSlice.duty_u16(SLICER_HALF_SPEED)
    time.sleep(0.05)
    pwmSlice.duty_u16(SLICER_THREE_QUARTER_SPEED)
    time.sleep(0.05)
    pwmSlice.duty_u16(SLICER_FULL_SPEED)
    time.sleep(0.6)
    pwmSlice.duty_u16(SLICER_STOP)
    time.sleep(0.25)
    print("sliced.")


def display_error_state():
    print("error state...")
    pwmSlice.duty_u16(SLICER_STOP)
    while True:
        pwmLED.duty_u16(LED_FULL_ON)
        time.sleep(0.5)
        pwmLED.duty_u16(LED_FULL_ON)
        lightsleep(3000)


def home_slicer(attempt_number=0):
    print(f"homing slicer, attempt {attempt_number + 1}/{HOMING_ATTEMPTS_LIMIT}...")
    if attempt_number == HOMING_ATTEMPTS_LIMIT:
        print("too many failed attempts to clear paper jam, giving up :(")
        display_error_state()
    start_time_ms = time.ticks_ms()
    time_elapsed_ms = time.ticks_ms() - start_time_ms
    pwmSlice.duty_u16(SLICER_QUARTER_SPEED)
    while pLimit.value() is not AT_LIMIT and time_elapsed_ms < HOMING_TIME_LIMIT_MS:
        time_elapsed_ms = time.ticks_ms() - start_time_ms
    if pLimit.value() is not AT_LIMIT:
        # probably a paper jam
        slice_paper()
        home_slicer(attempt_number + 1)
    pwmSlice.duty_u16(SLICER_STOP)
    print("slicer homed.")


def fade_in_leds(fade_time_ms):
    print("fading in LEDs...")
    start_time_ms = time.ticks_ms()
    brightness = (time.ticks_ms() - start_time_ms) / fade_time_ms
    while brightness < 1:
        brightness = (time.ticks_ms() - start_time_ms) / fade_time_ms
        pwmLED.duty_u16(min(int(LED_FULL_ON * brightness), 65535))
    pwmLED.duty_u16(LED_FULL_ON)
    print("LEDs on.")


def roll_paper():
    print("rolling paper...")
    pEN.value(STEPPER_ENABLED)

    microsteps = 0
    while microsteps < MICROSTEPS_PER_WISDOM:
        pSTEP.on()
        sleep_us(usDelay)
        pSTEP.off()
        sleep_us(usDelay)
        microsteps += 1
    pEN.value(STEPPER_DISABLED)
    print("paper rolled.")


def fade_out_leds(fade_time_ms):
    print("fading out LEDs...")
    start_time_ms = time.ticks_ms()
    brightness = 1 - (time.ticks_ms() - start_time_ms) / fade_time_ms
    while brightness > 0:
        brightness = 1 - (time.ticks_ms() - start_time_ms) / fade_time_ms
        pwmLED.duty_u16(max(int(LED_FULL_ON * brightness), 0))
    pwmLED.duty_u16(LED_FULL_OFF)
    print("LEDs off.")


def wait_for_button_release():
    print("waiting for button to be released...")
    while pGO.value() == GO_PRESSED:
        pass
    print("button released.")


def exchange_wisdom():
    print("exchanging wisdom...")
    fade_in_leds(LED_FADE_TIME_EXCHANGE_MS)
    roll_paper()
    slice_paper()
    home_slicer()
    wait_for_button_release()
    fade_out_leds(LED_FADE_TIME_EXCHANGE_MS)
    print("wisdom exchanged.")


def setup():
    print("running setup...")
    # steppers off
    pEN.value(STEPPER_DISABLED)
    pDIR.value(DIR_FORWARDS)

    # slicer off
    pwmSlice.duty_u16(SLICER_STOP)

    # leds on
    fade_in_leds(LED_FADE_TIME_EXCHANGE_MS)

    # slice any current paper
    slice_paper()

    # get slicer homed
    home_slicer()

    # leds off
    fade_out_leds(LED_FADE_TIME_EXCHANGE_MS)
    print("setup complete.")


def attract_mode_interrupt_callback(_):
    global do_exchange
    pGO.irq(None)
    print("button detected during attract mode")
    do_exchange = True


def run_attract_mode():
    global do_exchange
    print("running attract mode...")
    pGO.irq(handler=attract_mode_interrupt_callback, trigger=Pin.IRQ_RISING)
    fade_in_leds(LED_FADE_TIME_ATTRACT_MS)
    fade_out_leds(LED_FADE_TIME_ATTRACT_MS)
    pGO.irq(None)
    if do_exchange:
        do_exchange = False
        exchange_wisdom()
    print("attract done.")


def main_loop():
    while True:
        esp32.wake_on_ext0(pGO, esp32.WAKEUP_ANY_HIGH)  # wake on press
        lightsleep(ATTRACT_MODE_FLASH_EVERY_MS)  # not deepsleep because we need to hold stepper driver disabled, etc

        if machine.wake_reason() == machine.PIN_WAKE:
            print("woken up by machine.PIN_WAKE")
            exchange_wisdom()
        else:
            print("woken up by timer or something")
            run_attract_mode()


setup()
main_loop()
