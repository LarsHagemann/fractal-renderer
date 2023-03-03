#pragma once

#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

class Gradient {
public:
    using GradientKey = std::pair<double, sf::Color>;
    using Domain      = sf::Vector2<double>;
private:
    std::vector<GradientKey> m_keys;
    Domain                   m_domain;

    double NormalizeValue(double value) const {
        return (value - m_domain.x) / (m_domain.y - m_domain.x);
    }
    double UnnormalizeValue(double value) const {
        return value * (m_domain.y - m_domain.x) + m_domain.x;
    }
    void UnnormalizeAllKeys() {
        for (auto& key : m_keys) {
            key.first = UnnormalizeValue(key.first);
        }
    }
    void NormalizeAllKeys() {
        for (auto& key : m_keys) {
            key.first = NormalizeValue(key.first);
        }
    }
public:
    const GradientKey& GetKey(size_t index) {
        return m_keys[index];
    }
    void ModifyKeyColor(size_t index, const sf::Color& color) {
        m_keys[index].second = color;
    }
    size_t GetNumKeys() const {
        return m_keys.size();
    }
    void AddKey(GradientKey&& key) {
        key.first = NormalizeValue(key.first);

        // Find the correct position in m_keys to insert key
        for (auto i = 0u; i < m_keys.size(); ++i) {
            if (key.first < m_keys[i].first) {
                m_keys.insert(m_keys.begin() + i, key);
                return;
            }
        }

        m_keys.push_back(key);
    }

    void SetDomain(Domain domain, bool normalize = true) { 
        if (normalize) {
            UnnormalizeAllKeys();
            m_domain = domain;
            NormalizeAllKeys();
        }
        else {
            m_domain = domain;
        }
    }
    Domain GetDomain() const { return m_domain; }

    sf::Color GetColor(double value) const {
        // normalize value in domain
        value = (value - m_domain.x) / (m_domain.y - m_domain.x);
        for (auto i = 0u; i < m_keys.size(); ++i) {
            if (value < m_keys[i].first) {
                if (i == 0) {
                    return m_keys[0].second;
                }
                else {
                    const auto& key0 = m_keys[i - 1];
                    const auto& key1 = m_keys[i];
                    const auto  t    = (value - key0.first) / (key1.first - key0.first);
                    return sf::Color{
                        static_cast<sf::Uint8>((1 - t) * key0.second.r + key1.second.r * t),
                        static_cast<sf::Uint8>((1 - t) * key0.second.g + key1.second.g * t),
                        static_cast<sf::Uint8>((1 - t) * key0.second.b + key1.second.b * t),
                        static_cast<sf::Uint8>((1 - t) * key0.second.a + key1.second.a * t)
                    };
                }
            }
        }
        return m_keys.back().second;
    }
};
