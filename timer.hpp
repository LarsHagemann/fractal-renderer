#pragma once
#include <SFML/System/Clock.hpp>
#include <cstdio>

extern bool s_EnableTimerOutput;

class Timer {
private:
    std::string m_label;
    sf::Clock   m_clock;
public:
    Timer(const std::string& label) 
        : m_label(label)
    {}

    ~Timer() {
        if (s_EnableTimerOutput)
            printf_s("- %s took: %ums\n", m_label.data(), m_clock.getElapsedTime().asMilliseconds());
    }
};

