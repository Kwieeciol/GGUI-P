#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "color.h"

class Visualizer {
private:
    unsigned width;
    unsigned height;

    sf::RenderWindow window;
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;

public:
    Visualizer(unsigned image_width, double aspect_ratio)
        : width(image_width),
          height(std::max(1u, static_cast<unsigned>(image_width / aspect_ratio))),
          window(sf::VideoMode({width, height}), "Ray Tracer"),
          image({width, height}, sf::Color::Black),
          texture(sf::Vector2u{width, height}),
          sprite(texture)
    {}

    void on_scanline(int y, const std::vector<color>& row) {
        if (!window.isOpen()) return;

        for (unsigned x = 0; x < row.size(); x++) {
            const auto& c = row[x];

            image.setPixel({x, (unsigned)y}, sf::Color(
                static_cast<std::uint8_t>(255.999 * c.x()),
                static_cast<std::uint8_t>(255.999 * c.y()),
                static_cast<std::uint8_t>(255.999 * c.z())
            ));
        }

        texture.update(image);

        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear();
        window.draw(sprite);
        window.display();
    }

    unsigned get_height() const { return height; }
    unsigned get_width() const { return width; }

    void display() {
        while (window.isOpen()) {
            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>())
                    window.close();
            }

            window.clear();
            window.draw(sprite);
            window.display();
        }
    }
};

#endif
