#include <ArduinoSTL.h>
#include <vector>

using Pin = int;
using Timestamp = decltype(millis());
constexpr Timestamp millisInSecond = 1000;
constexpr Timestamp millisInMinute = millisInSecond * 60;
constexpr Timestamp millisInHour = millisInMinute * 60;
constexpr Timestamp fullDay = 24 * millisInHour;

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
                value = 0;
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

    int value() {return value_;}

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

std::vector<Segment> segments = {
    Segment(hoursRegister, Segment::Position::Left, 2),
    Segment(hoursRegister, Segment::Position::Right, 9),
    Segment(minutesRegister, Segment::Position::Left, 5),
    Segment(minutesRegister, Segment::Position::Right, 9),
    Segment(secondsResigter, Segment::Position::Left, 5),
    Segment(secondsResigter, Segment::Position::Right, 9)
};

/*
14:45:12
14 == 50400000
45 == 2700000
12 == 12000
*/
Timestamp setTime{0};
Timestamp deltaTime{0};
Timestamp lastPress{0};

auto configuringSegmentIdx = segments.end();
;

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
    segments[0].setValue(hour / 10);
    segments[1].setValue(hour % 10);
    segments[2].setValue(minute / 10);
    segments[3].setValue(minute % 10);
    segments[4].setValue(second / 10);
    segments[5].setValue(second % 10);
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
