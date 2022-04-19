#include <Arduino.h>
#include <jled.h>

#define PIN_RELAY_ECVT 4
#define PIN_RELAY_ALTERNATOR 9

#define PIN_INDICATOR_ECVT 11
#define PIN_INDICATOR_ALTERNATOR 10

#define PIN_TOGGLE_ECVT 8
#define PIN_TOGGLE_ALTERNATOR 7

#define PIN_BATTERY_SENSE A0

const float VOLTAGE_BATTERY_OVERRIDE = 12.;
const float VOLTAGE_OVERRIDE_DEACTIVATE = 13.;

const uint64_t ALTERNATOR_MIN_OVERRIDE_TIME = 10000;
const uint64_t ALTERNATOR_DEBOUNCE_INTERVAL = 1000;

const uint64_t NUM_AVG_SAMPLES = 20;
const uint64_t BATTERY_SAMPLE_INTERVAL = 250;

// MANUALLY CALIBRATED!!!
const float DIV_CONSTANT_BATTERY = 5. / 1024. / 0.2555;


float samples[NUM_AVG_SAMPLES];
size_t sample_idx;
uint64_t last_sample;

JLed indicator_alternator(PIN_INDICATOR_ALTERNATOR);
JLed indicator_ecvt(PIN_INDICATOR_ECVT);

float voltage_battery;

float read_battery_voltage() {
    return analogRead(PIN_BATTERY_SENSE) * DIV_CONSTANT_BATTERY;
}

float compute_avg_battery_voltage() {
    float sum = 0;
    for (auto i: samples) {
        sum += i;
    }
    return sum / (float) NUM_AVG_SAMPLES;
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_RELAY_ECVT, OUTPUT);
    pinMode(PIN_RELAY_ALTERNATOR, OUTPUT);

    pinMode(PIN_INDICATOR_ECVT, OUTPUT);
    pinMode(PIN_INDICATOR_ALTERNATOR, OUTPUT);

    pinMode(PIN_TOGGLE_ALTERNATOR, INPUT_PULLUP);
    pinMode(PIN_TOGGLE_ECVT, INPUT_PULLUP);

    pinMode(PIN_BATTERY_SENSE, INPUT);

}

void alternator_controller() {
    static unsigned long last_toggle;
    static enum {
        ACTIVE,
        ACTIVE_,

        ACTIVE_VOLTAGE_OVERRIDE,
        ACTIVE_VOLTAGE_OVERRIDE_,

        INACTIVE,
        INACTIVE_,
    } state = INACTIVE;

    switch (state) {
        case ACTIVE:
            if (millis() - last_toggle < ALTERNATOR_DEBOUNCE_INTERVAL) break;

            state = ACTIVE_;

            indicator_alternator.On().Forever();
            digitalWrite(PIN_RELAY_ALTERNATOR, HIGH);
            last_toggle = millis();

        case ACTIVE_:
            if (!digitalRead(PIN_TOGGLE_ALTERNATOR)) {
                state = INACTIVE;
            }
            break;

        case ACTIVE_VOLTAGE_OVERRIDE:
            state = ACTIVE_VOLTAGE_OVERRIDE_;
            indicator_alternator
                    .Blink(100, 100)
                    .Forever();

            digitalWrite(PIN_RELAY_ALTERNATOR, HIGH);
            last_toggle = millis();

        case ACTIVE_VOLTAGE_OVERRIDE_:
            if (millis() - last_toggle > ALTERNATOR_MIN_OVERRIDE_TIME &&
                voltage_battery >= VOLTAGE_OVERRIDE_DEACTIVATE) {
                state = ACTIVE;
            }
            break;

        case INACTIVE:
            if (millis() - last_toggle < ALTERNATOR_DEBOUNCE_INTERVAL) break;

            state = INACTIVE_;

            indicator_alternator.Off().Forever();
            digitalWrite(PIN_RELAY_ALTERNATOR, LOW);
            last_toggle = millis();

        case INACTIVE_:
            if (digitalRead(PIN_TOGGLE_ALTERNATOR)) {
                state = ACTIVE;
            }
            if (voltage_battery < VOLTAGE_BATTERY_OVERRIDE) {
                state = ACTIVE_VOLTAGE_OVERRIDE;
            }
            break;
    }

    Serial.println(state);
}

void loop() {
    if (millis() - last_sample > BATTERY_SAMPLE_INTERVAL) {
        last_sample = millis();
        samples[sample_idx++ % NUM_AVG_SAMPLES] = read_battery_voltage();
        voltage_battery = compute_avg_battery_voltage();
    }


    alternator_controller();

    indicator_alternator.Update();
    //delay(100);
}