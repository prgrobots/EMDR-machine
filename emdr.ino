#include <FastLED.h>
#include <TimerFreeTone.h>
#define LED_DATA_PIN 12
#define NUM_LEDS 30
#define LEFT_LED_PIN 11
#define RIGHT_LED_PIN 10
#define LEFT_PIEZO_PIN 9
#define RIGHT_PIEZO_PIN 8
#define VOLUME_POT_PIN A3
#define LEFT_RIGHT_LED_BRIGHTNESS_PIN A2
#define STRIP_BRIGHTNESS_POT_PIN A4
#define RED_POT_PIN A5
#define GREEN_POT_PIN A6
#define BLUE_POT_PIN A7
 
CRGB leds[NUM_LEDS];
int potSpeed = A0;
int potTone = A1;

void setup() {
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    pinMode(LEFT_LED_PIN, OUTPUT);
    pinMode(RIGHT_LED_PIN, OUTPUT);
    pinMode(LEFT_PIEZO_PIN, OUTPUT);
    pinMode(RIGHT_PIEZO_PIN, OUTPUT);
}

void loop() {
    int speed = map(analogRead(potSpeed), 0, 1023, 10, 50);
    int toneFrequency = map(analogRead(potTone), 0, 1023, 50, 2000);
    int volume = map(analogRead(VOLUME_POT_PIN), 0, 1023, 0, 10);
    int stripBrightness = map(analogRead(STRIP_BRIGHTNESS_POT_PIN), 0, 1023, 0, 255);
    int leftRightLedBrightness = map(analogRead(LEFT_RIGHT_LED_BRIGHTNESS_PIN), 0, 1023, 0, 255);
    int redValue = map(analogRead(RED_POT_PIN), 0, 1023, 0, 255);
    int greenValue = map(analogRead(GREEN_POT_PIN), 0, 1023, 0, 255);
    int blueValue = map(analogRead(BLUE_POT_PIN), 0, 1023, 0, 255);

    FastLED.setBrightness(stripBrightness);
    CRGB color = CRGB(redValue, greenValue, blueValue);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
        FastLED.show();
        leds[i] = CRGB::Black;
        delay(speed);

        if (i == 0) {
            digitalWrite(LEFT_LED_PIN, HIGH);
            analogWrite(RIGHT_LED_PIN, leftRightLedBrightness);
            TimerFreeTone(LEFT_PIEZO_PIN, toneFrequency, 200, volume);
        } else {
            digitalWrite(LEFT_LED_PIN, LOW);
            noTone(LEFT_PIEZO_PIN);
            digitalWrite(RIGHT_LED_PIN, LOW);
        }
    }

    for (int i = NUM_LEDS - 1; i >= 0; i--) {
        leds[i] = color;
        FastLED.show();
        leds[i] = CRGB::Black;
        delay(speed);

        if (i == NUM_LEDS - 1) {
            digitalWrite(RIGHT_LED_PIN, HIGH);
            analogWrite(LEFT_LED_PIN, leftRightLedBrightness);
            TimerFreeTone(RIGHT_PIEZO_PIN, toneFrequency, 200, volume);
        } else {
            digitalWrite(RIGHT_LED_PIN, LOW);
            noTone(RIGHT_PIEZO_PIN);
            digitalWrite(LEFT_LED_PIN, LOW);
        }
    }
}
