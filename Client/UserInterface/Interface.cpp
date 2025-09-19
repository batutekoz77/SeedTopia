#include "Interface.hpp"

UIButton::UIButton(const sf::Font& font, const std::string& text, unsigned int charSize, const sf::Vector2f& size, int r, int g, int b) {
    rect.setSize(size);
    rect.setFillColor(sf::Color(r, g, b));
   
    label = std::make_unique<sf::Text>(font, text, charSize);
    label->setFillColor(sf::Color::White);
}

void UIButton::setPosition(const sf::Vector2f& pos) {
    rect.setPosition(pos);

    auto lb = label->getLocalBounds();
    label->setPosition(
        sf::Vector2f(
            pos.x + (rect.getSize().x - lb.size.x) / 2.f - lb.position.x,
            pos.y + (rect.getSize().y - lb.size.y) / 2.f - lb.position.y
        )
    );
}

bool UIButton::contains(const sf::Vector2f& p) const {
    return rect.getGlobalBounds().contains(p);
}

void UIButton::handleEvent(const sf::Event::MouseButtonPressed& ev, sf::RenderWindow& window) {
    if (ev.button == sf::Mouse::Button::Left) {
        sf::Vector2f pos = window.mapPixelToCoords(ev.position);
        if (contains(pos)) pressed = true;
    }
}

void UIButton::handleEvent(const sf::Event::MouseButtonReleased& ev, sf::RenderWindow& window) {
    if (pressed && ev.button == sf::Mouse::Button::Left) {
        sf::Vector2f pos = window.mapPixelToCoords(ev.position);
        if (contains(pos)) {
            if (callback) callback();
        }
    }
    pressed = false;
}

void UIButton::draw(sf::RenderWindow& window) {
    window.draw(rect);
    window.draw(*label);
}





TextInput::TextInput(const sf::Font& font, const sf::Vector2f& position, unsigned int charSize) {
    box.setSize({ 300.f, 40.f });
    box.setFillColor(sf::Color(50, 50, 50));
    box.setOutlineColor(sf::Color::White);
    box.setOutlineThickness(2.f);
    box.setPosition(position);
    
    text = std::make_unique<sf::Text>(font, "", charSize);
    text->setFillColor(sf::Color::White);
    text->setPosition(position + sf::Vector2f(5.f, 5.f));
}

void TextInput::handleEvent(const sf::Event::MouseButtonPressed& ev, sf::RenderWindow& window) {
    sf::Vector2f mousePos(sf::Vector2f(ev.position.x, ev.position.y));
    focused = box.getGlobalBounds().contains(mousePos);
}

void TextInput::handleEvent(const sf::Event::MouseButtonReleased& ev, sf::RenderWindow& window) {
    // Currently empty; could add visual feedback
}

void TextInput::handleEvent(const sf::Event::TextEntered& ev) {
    if (!focused) return;

    if (ev.unicode == 8) {
        if (!str.empty()) str.pop_back();
    }
    else if (ev.unicode < 128) {
        str += static_cast<char>(ev.unicode);
    }
    text->setString(str);
}

void TextInput::draw(sf::RenderWindow& window) {
    window.draw(box);
    window.draw(*text);
}