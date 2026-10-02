#pragma once
#include <SDL.h>
#include <map>
#include <string>

namespace ashford {
std::wstring wide(const std::string& text);
class Text {
    SDL_Renderer* renderer;
    std::map<std::string, SDL_Texture*> cache;
public:
    explicit Text(SDL_Renderer* value) : renderer(value) {}
    ~Text();
    void draw(const std::string& value, int x, int y, int size, SDL_Color color,
              bool serif = false, bool bold = false, int max_width = 1000);
    void clear();
};
}
