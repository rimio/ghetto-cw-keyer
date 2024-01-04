// PIN configuration
static const int PIN_7SEG_A = 0;
static const int PIN_7SEG_G = 6;
static const int PIN_7SEG_SINK_0 = 9;
static const int PIN_7SEG_SINK_1 = 8;

static const int PIN_ENCODER_BTN = 11;
static const int PIN_ENCODER_A = 12;
static const int PIN_ENCODER_B = 13;

static const int PIN_WAVE = 10;

static const int PIN_LEFT = 14;
static const int PIN_RIGHT = 15;

// debounced pin state
struct pin_state_t {
    int value;
    unsigned long waitUntilUs;
    bool steady;
};

static const unsigned long DEBOUNCE_TIME_US = 500;

// digits definition
static const int DIGITS[16][7] = {
    { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW },
    { LOW, HIGH, HIGH, LOW, LOW, LOW, LOW },
    { HIGH, HIGH, LOW, HIGH, HIGH, LOW, HIGH },
    { HIGH, HIGH, HIGH, HIGH, LOW, LOW, HIGH },
    { LOW, HIGH, HIGH, LOW, LOW, HIGH, HIGH },
    { HIGH, LOW, HIGH, HIGH, LOW, HIGH, HIGH },
    { HIGH, LOW, HIGH, HIGH, HIGH, HIGH, HIGH },
    { HIGH, HIGH, HIGH, LOW, LOW, LOW, LOW },
    { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH },
    { HIGH, HIGH, HIGH, HIGH, LOW, HIGH, HIGH },
    { LOW, LOW, LOW, HIGH, HIGH, HIGH, HIGH },
    { HIGH, LOW, LOW, HIGH, HIGH, HIGH, HIGH },
    { LOW, LOW, LOW, LOW, HIGH, LOW, HIGH },
    { LOW, LOW, LOW, LOW, HIGH, HIGH, LOW },
    { HIGH, HIGH, HIGH, LOW, HIGH, HIGH, HIGH },
    { LOW, LOW, HIGH, HIGH, HIGH, HIGH, HIGH },
};

// work mode
unsigned const int MODE_STRAIGHT = 0;
unsigned const int MODE_IAMBIC_A = 1;
unsigned const int MODE_IAMBIC_B = 2;
unsigned const int NUM_MODES = 3;

unsigned int mode = MODE_STRAIGHT;

static unsigned int wpm = 10;
static unsigned int ditDurationMs = 60000 / 50 / wpm;

static bool toneOn = false;
static unsigned long toneUntilMs = 0;
static unsigned long pauseUntilMs = 0;

typedef enum {
    NONE = 0,
    DIT = 1,
    DAH = 2
} iambic_sym_t;

static iambic_sym_t current = NONE;
static iambic_sym_t next = NONE;

// display
static unsigned int digits[2] = { 0, 0 };
static unsigned int lastSeg = 0;

static const unsigned int DISPLAY_TIMEOUT_MS = 2000;
static unsigned int displayWaitForMs = 0;

// rotary state
pin_state_t rotAState;
pin_state_t rotBState;
pin_state_t rotBtnState;

static int encLastState = 0;
static int btnLastState = 0;
static int encoderValue = 20;

void display(u8 digit, u8 seg)
{
    for (int i = PIN_7SEG_A; i <= PIN_7SEG_G; i ++) {
        digitalWrite(i, LOW);
    }

    if (seg == 0) {
        digitalWrite(PIN_7SEG_SINK_0, HIGH);
        digitalWrite(PIN_7SEG_SINK_1, LOW);
    } else {
        digitalWrite(PIN_7SEG_SINK_0, LOW);
        digitalWrite(PIN_7SEG_SINK_1, HIGH);
    }

    for (int i = PIN_7SEG_A; i <= PIN_7SEG_G; i ++) {
        digitalWrite(i, DIGITS[digit][i]);
    }
}

void numberToDigits(unsigned int num)
{
    digits[0] = num % 10;
    digits[1] = (num / 10) % 10;
}

void debounce(unsigned long nowUs, int pin, pin_state_t *state)
{
    int v = digitalRead(pin);
    if (v == state->value) {
        if (state->waitUntilUs <= nowUs) {
            state->steady = true;
        }
    } else {
        state->value = v;
        state->waitUntilUs = nowUs + DEBOUNCE_TIME_US;
        state->steady = false;
    }
}

void pwm(unsigned char duty, float freq)
{
    // straight up stolen from the Arduino forum, #noshame
    TCCR1A = 0x21;
    TCCR1B = 0x14;
    OCR1A = 0x7A12 / freq;
    OCR1B = OCR1A * (duty / 255.0);
}

void setup() {
    for (int i = PIN_7SEG_A; i <= PIN_7SEG_G; i++) {
        pinMode(i, OUTPUT);
    }
    pinMode(PIN_7SEG_SINK_0, OUTPUT);
    pinMode(PIN_7SEG_SINK_1, OUTPUT);

    pinMode(PIN_ENCODER_A, INPUT);
    pinMode(PIN_ENCODER_B, INPUT);
    pinMode(PIN_ENCODER_BTN, INPUT);

    pinMode(PIN_WAVE, OUTPUT);

    pinMode(PIN_LEFT, INPUT);
    pinMode(PIN_RIGHT, INPUT);
}

void loop()
{
    unsigned long now = millis();
    unsigned long nowUs = micros();

    // rotary encoder debounce
    debounce(nowUs, PIN_ENCODER_A, &rotAState);
    debounce(nowUs, PIN_ENCODER_B, &rotBState);
    debounce(nowUs, PIN_ENCODER_BTN, &rotBtnState);

    // mode switching
    if (rotBtnState.steady) {
        if (rotBtnState.value == LOW && btnLastState == HIGH) {
            mode ++;
            mode %= NUM_MODES;
            displayWaitForMs = now + DISPLAY_TIMEOUT_MS;
        }
        btnLastState = rotBtnState.value;
    }

    // wpm switching
    if (rotAState.steady && rotBState.steady) {
        if (mode != MODE_STRAIGHT && rotAState.value != encLastState) {
            if (rotAState.value != rotBState.value) {
                encoderValue ++;
                displayWaitForMs = 0;
            } else {
                encoderValue --;
                displayWaitForMs = 0;
            }
        }
        encLastState = rotAState.value;
        encoderValue = encoderValue > 100 ? 100 : encoderValue;
        encoderValue = encoderValue < 2 ? 2 : encoderValue;
        wpm = encoderValue / 2;
        ditDurationMs = 60000 / 50 / wpm;
    }

    // determine what we display
    switch (mode) {
        case MODE_STRAIGHT:
            // display St
            digits[0] = 10;
            digits[1] = 5;
            break;

        case MODE_IAMBIC_A:
            if (displayWaitForMs > now) {
                // display IA
                digits[0] = 14;
                digits[1] = 13;
            } else {
                numberToDigits(wpm);
            }
            break;

        case MODE_IAMBIC_B:
            if (displayWaitForMs > now) {
                // display IB
                digits[0] = 15;
                digits[1] = 13;
            } else {
                numberToDigits(wpm);
            }
            break;

        default:
            // error, should not get here, display Er
            digits[0] = 12;
            digits[1] = 11;
            break;
    }

    // display stuff
    lastSeg ++;
    lastSeg %= 2;
    display(digits[lastSeg], lastSeg);

    // keying
    int left = digitalRead(PIN_LEFT);
    int right = digitalRead(PIN_RIGHT);

    // we need updated timer for important crap
    now = millis();

    switch (mode) {
        case MODE_STRAIGHT:
            toneOn = (left == LOW);
            break;

        case MODE_IAMBIC_A:
        case MODE_IAMBIC_B:
            {
                // determine dit/dah memory
                bool doDit = (left == LOW);
                bool doDah = (right == LOW);
                if (current == NONE) {
                    if (doDit) {
                        next = DIT;
                    } else if (doDah) {
                        next = DAH;
                    }
                } else if (next == NONE) {
                    if (current == DIT && doDah) {
                        next = DAH;
                    } else if (current == DAH && doDit) {
                        next = DIT;
                    }
                }

                // handle release in iambic A mode
                if (mode == MODE_IAMBIC_A && !doDit && !doDah) {
                    next = NONE;
                }

                // tone timing
                if (!toneOn && (pauseUntilMs < now)) {
                    // advance
                    current = next;
                    next = NONE;
                    // enqueue dit or dah
                    if (current == DIT) {
                        toneOn = true;
                        toneUntilMs = now + ditDurationMs;
                    } else if (current == DAH) {
                        toneOn = true;
                        toneUntilMs = now + ditDurationMs * 3;
                    }
                } else if (toneOn && (toneUntilMs < now)) {
                    toneOn = false;
                    pauseUntilMs = now + ditDurationMs;
                }
            }
            break;

        default:
            break;
    }

    // tone synthesis
    pwm(toneOn ? 128 : 0, 700);
}
