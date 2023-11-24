using Pin = int;
using Timestamp = decltype(millis());
constexpr Timestamp millisInSecond = 1000;
constexpr Timestamp millisInMinute = millisInSecond * 60;
constexpr Timestamp millisInHour = millisInMinute * 60;
constexpr Timestamp fullDay = 24 * millisInHour;

struct Mode
{
    enum Value {
        Clock,
        SetupHour1,
        SetupHour2,
        SetupMinute1,
        SetupMinute2,
        SetupSecond1,
        SetupSecond2,
        LastValue
    };
};

namespace pins
{
Pin button1 = 2;
Pin button2 = 3;
} // namespace pins

class Segment
{
public:
    Segment(Pin data, Pin latch, Pin clock)
        : data_(data)
        , latch_(latch)
        , clock_(clock)
    {
        pinMode(data_, OUTPUT);
        pinMode(latch_, OUTPUT);
        pinMode(clock_, OUTPUT);
    }

    void display()
    {
        if (blinkpart_ != 0) {
            int blinkTimeout = blinkCycle_ ? 100 : 1000;
            Timestamp now = millis();
            if (now < lastChange_ || now - lastChange_ > blinkTimeout) {
                blinkCycle_ = !blinkCycle_;
                lastChange_ = now;
            }
        }
        int res{};
        int tens = value_ / 10;
        int digits = value_ % 10;
        res = tens | (digits << 4);

        if (blinkpart_ == 1) {
            if (!valueSet_) {
                res |= 0b00001111;
            }
            if (blinkCycle_) {
                res &= 0b11110000;
            }
        } else if (blinkpart_ == 2) {
            if (!valueSet_) {
                res |= 0b11110000;
            }
            if (blinkCycle_) {
                res &= 0b00001111;
            }
        }
        digitalWrite(latch_, LOW);
        shiftOut(data_, clock_, MSBFIRST, res);
        digitalWrite(latch_, HIGH);
    }

    void setValue(int value)
    {
        value_ = value;
        valueSet_ = true;
    }

    int value() const
    {
        return value_;
    }

    void setBlink(int part)
    {
        if (blinkpart_ != part) {
            valueSet_ = false;
            lastChange_ = 0;
        }
        blinkpart_ = part;
    }

private:
    Pin data_;
    Pin latch_;
    Pin clock_;

    int value_;
    int blinkpart_{0};
    Timestamp lastChange_{};
    bool blinkCycle_ = true;
    bool valueSet_ = false;
};

Segment hours{11, 5, 8};
Segment minutes{12, 6, 9};
Segment seconds{13, 7, 10};

Mode::Value mode = Mode::Clock;

/*
14:45:12
14 == 50400000
45 == 2700000
12 == 12000
*/
Timestamp setTime{0};
Timestamp deltaTime{0};
Timestamp lastPress{0};

void cycleMode()
{
    mode = static_cast<Mode::Value>((mode + 1) % Mode::LastValue);
}

void button1ISR()
{
    noInterrupts();
    if (millis() - lastPress < 150) {
        interrupts();
        return;
    }
    lastPress = millis();
    Serial.println("Button 1: ");
    cycleMode();
    if (mode == Mode::Clock) {
        setTime = hours.value() * millisInHour + minutes.value() * millisInMinute +
                  seconds.value() * millisInSecond;
        deltaTime = millis();
    }
    Serial.println(mode);
    interrupts();
}

void button2ISR()
{
    noInterrupts();
    if (millis() - lastPress < 150) {
        interrupts();
        return;
    }
    lastPress = millis();
    Serial.println("Button 2: ");
    switch (mode) {
        case Mode::SetupHour1: {
            int newValue = hours.value();
            if (newValue / 10 == 2) {
                newValue -= 20;
            } else {
                newValue += 10;
            }
            hours.setValue(newValue);
        } break;
        case Mode::SetupHour2: {
            int newValue = hours.value();
            if (newValue % 10 == 9) {
                newValue -= 9;
            } else {
                ++newValue;
            }
            hours.setValue(newValue);
        } break;
        case Mode::SetupMinute1: {
            int newValue = minutes.value();
            if (newValue / 10 == 5) {
                newValue -= 50;
            } else {
                newValue += 10;
            }
            minutes.setValue(newValue);
        } break;
        case Mode::SetupMinute2: {
            int newValue = minutes.value();
            if (newValue % 10 == 9) {
                newValue -= 9;
            } else {
                ++newValue;
            }
            minutes.setValue(newValue);
        } break;
        case Mode::SetupSecond1: {
            int newValue = seconds.value();
            if (newValue / 10 == 5) {
                newValue -= 50;
            } else {
                newValue += 10;
            }
            seconds.setValue(newValue);
        } break;
        case Mode::SetupSecond2: {
            int newValue = seconds.value();
            if (newValue % 10 == 9) {
                newValue -= 9;
            } else {
                ++newValue;
            }
            seconds.setValue(newValue);
        } break;
    }
    interrupts();
}

void setup()
{
    int startHour = 0;
    int startMinute = 0;
    int startSecond = 0;
    setTime =
        startHour * millisInHour + startMinute * millisInMinute + startSecond * millisInSecond;

    pinMode(pins::button1, INPUT);
    digitalWrite(pins::button1, HIGH);
    attachInterrupt(digitalPinToInterrupt(pins::button1), button1ISR, FALLING);

    pinMode(pins::button2, INPUT);
    digitalWrite(pins::button2, HIGH);
    attachInterrupt(digitalPinToInterrupt(pins::button2), button2ISR, FALLING);

    Serial.begin(115200);
    Serial.println("Started");
}

void clockTick()
{
    Timestamp now = millis() + setTime - deltaTime;
    while (now > fullDay) {
        now -= fullDay;
    }

    int hour = now / millisInHour;
    now -= hour * millisInHour;
    int minute = now / millisInMinute;
    now -= minute * millisInMinute;
    int second = now / millisInSecond;

    hours.setValue(hour);
    minutes.setValue(minute);
    seconds.setValue(second);
}

void loop()
{
    switch (mode) {
        case Mode::Clock:
            hours.setBlink(0);
            minutes.setBlink(0);
            seconds.setBlink(0);
            clockTick();
            break;
        case Mode::SetupHour1:
            hours.setBlink(1);
            minutes.setBlink(0);
            seconds.setBlink(0);
            break;
        case Mode::SetupHour2:
            hours.setBlink(2);
            minutes.setBlink(0);
            seconds.setBlink(0);
            break;
        case Mode::SetupMinute1:
            hours.setBlink(0);
            minutes.setBlink(1);
            seconds.setBlink(0);
            break;
        case Mode::SetupMinute2:
            hours.setBlink(0);
            minutes.setBlink(2);
            seconds.setBlink(0);
            break;
        case Mode::SetupSecond1:
            hours.setBlink(0);
            minutes.setBlink(0);
            seconds.setBlink(1);
            break;
        case Mode::SetupSecond2:
            hours.setBlink(0);
            minutes.setBlink(0);
            seconds.setBlink(2);
            break;
    }

    hours.display();
    minutes.display();
    seconds.display();
    delay(100);
}
