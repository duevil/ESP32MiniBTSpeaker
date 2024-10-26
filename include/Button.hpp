#ifndef BUTTON_HPP
#define BUTTON_HPP

#include <functional>
#include <Bounce2.h>


class Button : public Bounce2::Button {
    static constexpr uint16_t LONG_PRESS_DURATION = 330;
public:
    using Callback = std::function<void()>;

    explicit Button(uint8_t pin, Callback shortPress, Callback longPress, uint16_t interval_millis = 5)
            : Bounce2::Button(), shortPress(std::move(shortPress)), longPress(std::move(longPress)) {
        this->pin = pin;
        interval(interval_millis);
        setPressedState(LOW);
    }

    virtual ~Button() = default;

    void setup() { attach(pin, INPUT_PULLUP); }

    void loop() {
        static bool long_press = false;
        update();
        if (isPressed() && !long_press && currentDuration() > LONG_PRESS_DURATION) {
            long_press = true;
            if (longPress) longPress();
        }
        if (released()) {
            long_press = false;
            if (previousDuration() < LONG_PRESS_DURATION && shortPress) shortPress();
        }
    }

private:
    Callback shortPress;
    Callback longPress;
};


#endif //BUTTON_HPP
