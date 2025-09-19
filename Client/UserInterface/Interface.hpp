#pragma once
#include <SFML/Graphics.hpp>
#include <fstream>
#include <string>
#include <functional>
#include <memory>
#include <cstdint>

class UIButton {
public:
    UIButton(const sf::Font& font, const std::string& text, unsigned int charSize, const sf::Vector2f& size, int r, int g, int b);

    void setPosition(const sf::Vector2f& pos);
    void handleEvent(const sf::Event::MouseButtonPressed& ev, sf::RenderWindow& window);
    void handleEvent(const sf::Event::MouseButtonReleased& ev, sf::RenderWindow& window);
    void draw(sf::RenderWindow& window);

    bool isPressed() const { return pressed; }
    void setCallback(std::function<void()> cb) { callback = std::move(cb); }

private:
    sf::RectangleShape rect;
    std::unique_ptr<sf::Text> label;
    bool pressed = false;
    std::function<void()> callback;

    bool contains(const sf::Vector2f& p) const;
};

class TextInput {
public:
    TextInput(const sf::Font& font, const sf::Vector2f& position, unsigned int charSize, const sf::Vector2f& size);

    void handleEvent(const sf::Event::MouseButtonPressed& ev, sf::RenderWindow& window);
    void handleEvent(const sf::Event::MouseButtonReleased& ev, sf::RenderWindow& window);
    void handleEvent(const sf::Event::TextEntered& ev);

    sf::Vector2f getPosition() const { return box.getPosition(); }
    void draw(sf::RenderWindow& window);

    const std::string& getText() const { return str; }
    void setText(const std::string& s) { str = s; text->setString(str); }

private:
    sf::RectangleShape box;
    std::unique_ptr<sf::Text> text;
    std::string str;

    bool focused = false;
};