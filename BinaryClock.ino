#include <ArduinoSTL.h>
#include <vector>
#include <array>

using Pin = int;
using Timestamp = decltype(millis());
constexpr Timestamp millisInSecond = 1000;
constexpr Timestamp millisInMinute = millisInSecond * 60;
constexpr Timestamp millisInHour = millisInMinute * 60;
constexpr Timestamp fullDay = 24 * millisInHour;

long adjustedMillis()
{
    auto res = millis();
    //res += res * 8. / 2340.;
    //res += res * (135. / 45976. + 8. / 18321. - 1. / 56338.);
    //res += res * (135. / 45976. + 8. / 18321. - 6. / 78185.);
    res += res * (380. / 129600. + 3./6854.);
    return res;
}

namespace pins
{
Pin button1 = 2;
Pin button2 = 3;
} // namespace pins

class ShiftRegister
{
public:
    ShiftRegister(Pin data, Pin latch, Pin clock)
        : data_(data)
        , latch_(latch)
        , clock_(clock)
    {
        pinMode(data_, OUTPUT);
        pinMode(latch_, OUTPUT);
        pinMode(clock_, OUTPUT);
    }

    void setValue(int value, int mask)
    {
        value_ &= ~mask;
        value_ |= (value & mask);
    }
    void display()
    {
        digitalWrite(latch_, LOW);
        shiftOut(data_, clock_, MSBFIRST, value_);
        digitalWrite(latch_, HIGH);
    }

private:
    int value_{};
    Pin data_;
    Pin latch_;
    Pin clock_;
};

class Segment
{
public:
    enum class Position { Left, Right };
    enum class BlinkMode { None, Full, Value };
    Segment(ShiftRegister& shiftRegister, Position position, int maxValue)
        : shiftRegister_(shiftRegister)
        , position_(position)
        , maxValue_(maxValue)
    {
    }

    void setValue(int value)
    {
        value_ = value;
    }

    void display()
    {
        int value = 0;
        if (blinkMode_ == BlinkMode::None || blinkMode_ == BlinkMode::Value) {
            value = value_;
        } else if (blinkMode_ == BlinkMode::Full) {
            value = 0b11111111;
        }
        if (blinkMode_ != BlinkMode::None) {
            Timestamp now = millis();
            Timestamp blinkTimeout = blinkPhase_ ? 100 : 1000;
            if (now < lastBlink_ || now - lastBlink_ > blinkTimeout) {
                lastBlink_ = now;
                if (blinkPhase_) {
                    value = 0;
                }
                blinkPhase_ = !blinkPhase_;
            }
        }
        if (position_ == Position::Right) {
            value <<= 4;
        }
        shiftRegister_.setValue(value, position_ == Position::Left ? leftMask_ : rightMask_);
        shiftRegister_.display();
    }

    void setBlinkMode(BlinkMode mode)
    {
        blinkMode_ = mode;
    }

    void increaseValue()
    {
        ++value_;
        if (value_ > maxValue_) {
            value_ = 0;
        }
    }

    int value()
    {
        return value_;
    }

private:
    static constexpr int leftMask_ = 0b00001111;
    static constexpr int rightMask_ = 0b11110000;
    ShiftRegister& shiftRegister_;
    Position position_;
    int value_{};
    int maxValue_;
    BlinkMode blinkMode_{BlinkMode::None};
    Timestamp lastBlink_{};
    bool blinkPhase_{true};
};

ShiftRegister hoursRegister{11, 5, 8};
ShiftRegister minutesRegister{12, 6, 9};
ShiftRegister secondsResigter{13, 7, 10};

std::vector<Segment> segments = {Segment(hoursRegister, Segment::Position::Left, 2),
    Segment(hoursRegister, Segment::Position::Right, 9),
    Segment(minutesRegister, Segment::Position::Left, 5),
    Segment(minutesRegister, Segment::Position::Right, 9),
    Segment(secondsResigter, Segment::Position::Left, 5),
    Segment(secondsResigter, Segment::Position::Right, 9)};

/*
14:45:55
14 == 50400000
45 == 2700000
55 == 55000
*/
Timestamp setTime{0};
Timestamp deltaTime{0};
Timestamp lastPress{0};

auto configuringSegmentIdx = segments.end();
bool displaySeconds = true;

void button1ISR()
{
    noInterrupts();
    if (millis() - lastPress < 200) {
        interrupts();
        return;
    }
    lastPress = millis();
    Serial.println("Button1");
    if (configuringSegmentIdx == segments.end()) {
        configuringSegmentIdx = segments.begin();
    } else {
        configuringSegmentIdx->setBlinkMode(Segment::BlinkMode::None);
        ++configuringSegmentIdx;
    }
    if (configuringSegmentIdx == segments.end()) {
        int hour = segments[0].value() * 10 + segments[1].value();
        int minute = segments[2].value() * 10 + segments[3].value();
        int second = segments[4].value() * 10 + segments[5].value();
        setTime = hour * millisInHour + minute * millisInMinute + second * millisInSecond;
        deltaTime = adjustedMillis();
    } else {
        configuringSegmentIdx->setBlinkMode(Segment::BlinkMode::Full);
    }

    interrupts();
}

void button2ISR()
{
    noInterrupts();
    if (millis() - lastPress < 200) {
        interrupts();
        return;
    }
    lastPress = millis();
    Serial.println("Button2");
    if (configuringSegmentIdx != segments.end()) {
        configuringSegmentIdx->increaseValue();
        configuringSegmentIdx->setBlinkMode(Segment::BlinkMode::Value);
    } else {
        displaySeconds = !displaySeconds;
    }
    interrupts();
}

void setup()
{
    int startHour = 14;
    int startMinute = 45;
    int startSecond = 55;
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

void animationShiftLeft(int h, int m, int oldH, int oldM)
{
    constexpr int delay1 = 150;
    constexpr int seconds = delay1 * 7 / 1000;
    std::array<int, 12> values{oldH / 10, oldH % 10, oldM / 10, oldM % 10, 5, 9, h / 10, h % 10,
        m / 10, m % 10, 0, seconds};
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < values.size() - 1; ++j) {
            if (j < segments.size()) {
                segments[j].setValue(values[j]);
                segments[j].display();
            }
            values[j] = values[j + 1];
        }
        delay(delay1);
    }
}

void animationShiftTop(int h, int m, int oldH, int oldM)
{
    auto makeDigit = [](int oldValue, int newValue) { return (oldValue << 4) | newValue; };
    constexpr int delay1 = 150;
    constexpr int seconds = delay1 * 4 / 1000;
    std::array<int, 6> values{makeDigit(oldH / 10, h / 10), makeDigit(oldH % 10, h % 10),
        makeDigit(oldM / 10, m / 10), makeDigit(oldM % 10, m % 10), makeDigit(5, 0),
        makeDigit(9, seconds)};

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 6; ++j) {
            segments[j].setValue((values[j] >> 4));
            segments[j].display();
            values[j] <<= 1;
        }
        delay(delay1);
    }
}

void animationFadeTop(int h, int m, int oldH, int oldM)
{
    constexpr int delay1 = 30;
    constexpr int delay2 = 100;
    constexpr int newSeconds = (delay1 * 6 * 8 + delay2 * 8) / 1000;
    std::array<int, 6> oldValues{oldH / 10, oldH % 10, oldM / 10, oldM % 10, 5, 9};
    std::array<int, 6> newValues{h / 10, h % 10, m / 10, m % 10, 0, newSeconds};
    int val = 0;
    for (int i = 0; i < 4; ++i) {
        val |= 1 << i;
        for (int j = 0; j < 6; ++j) {
            auto& segment = segments[j];
            segment.setValue(val | oldValues[j]);
            segment.display();
            delay(delay1);
        }
        delay(delay2);
    }
    val = 0b1111;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 6; ++j) {
            auto& segment = segments[j];
            segment.setValue(val | newValues[j]);
            segment.display();
            delay(delay1);
        }
        delay(delay2);
        val <<= 1;
    }
}

void animationFadeLeft(int h, int m, int oldH, int oldM)
{
    constexpr int delay1 = 30;
    constexpr int delay2 = 100;
    constexpr int seconds = (delay1 * 12 * 4 + delay2 * 12) / 1000;
    std::array<int, 6> oldValues{oldH / 10, oldH % 10, oldM / 10, oldM % 10, 5, 9};
    std::array<int, 6> newValues{h / 10, h % 10, m / 10, m % 10, 0, seconds};
    for (int i = 0; i < 6; ++i) {
        auto& segment = segments[i];
        int val = 0;
        for (int j = 0; j < 4; ++j) {
            val |= 1 << j;
            segment.setValue(val | oldValues[i]);
            segment.display();
            delay(delay1);
        }
        delay(delay2);
    }
    for (int i = 0; i < 6; ++i) {
        auto& segment = segments[i];
        int val = 0b1111;
        for (int j = 0; j < 4; ++j) {
            val <<= 1;
            segment.setValue(val | newValues[i]);
            segment.display();
            delay(delay1);
        }
        delay(delay2);
    }
}

void animationLineLeft(int h, int m, int oldH, int oldM)
{
    constexpr int delay1 = 150;
    constexpr int seconds = delay1 * 14 / 1000;
    std::array<int, 6> values{h / 10, h % 10, m / 10, m % 10, 0, seconds};
    for (int i = 0; i < 7; ++i) {
        if (i < 6) {
            segments[i].setValue(0b1111);
            segments[i].display();
        }
        if (i > 0) {
            segments[i - 1].setValue(0);
            segments[i - 1].display();
        }
        delay(delay1);
    }
    for (int i = 0; i < 7; ++i) {
        if (i < 6) {
            segments[i].setValue(0b1111);
            segments[i].display();
        }
        if (i > 0) {
            segments[i - 1].setValue(values[i - 1]);
            segments[i - 1].display();
        }
        delay(delay1);
    }
}

int lastMinute = 0;
int lastHour = 0;

using Animation = void (*)(int, int, int, int);

std::vector<Animation> animations = {&animationFadeLeft, &animationFadeTop, &animationShiftLeft,
    &animationShiftTop, &animationLineLeft};
//std::vector<Animation> animations = {&animationShiftTop};

void clockTick()
{
    Timestamp now = adjustedMillis();
    now = now + setTime - deltaTime;
    while (now > fullDay) {
        now -= fullDay;
    }

    int hour = now / millisInHour;
    now -= hour * millisInHour;
    int minute = now / millisInMinute;
    now -= minute * millisInMinute;
    int second = now / millisInSecond;
    if (minute != lastMinute) {
        if (displaySeconds) {
            auto animation = animations[rand() % animations.size()];
            animation(hour, minute, lastHour, lastMinute);
            lastMinute = minute;
            lastHour = hour;
            return;
        }
        lastMinute = minute;
        lastHour = hour;
    }
    segments[0].setValue(hour / 10);
    segments[1].setValue(hour % 10);
    segments[2].setValue(minute / 10);
    segments[3].setValue(minute % 10);
    if (displaySeconds) {
        segments[4].setValue(second / 10);
        segments[5].setValue(second % 10);
    } else {
        segments[4].setValue(0);
        segments[5].setValue(0);
    }
}

void loop()
{
    if (configuringSegmentIdx == segments.end()) {
        clockTick();
    }
    for (auto& s : segments) {
        s.display();
    }
    delay(100);
}
