#include "text.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ashford {
std::wstring wide(const std::string& text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count && !text.empty()) throw std::runtime_error("Invalid UTF-8 text.");
    std::wstring result(count, L'\0');
    if (count) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), count);
    return result;
}
Text::~Text() { clear(); }
void Text::clear() { for (auto& item : cache) SDL_DestroyTexture(item.second); cache.clear(); }
void Text::draw(const std::string& value, int x, int y, int size, SDL_Color color, bool serif, bool bold, int max_width) {
    if (value.empty()) return;
    std::string key = value + "|" + std::to_string(size) + (serif ? "S" : "N") + (bold ? "B" : "R") + std::to_string(max_width);
    auto found = cache.find(key);
    SDL_Texture* texture;
    if (found == cache.end()) {
        if (cache.size() > 256) clear();
        auto text = wide(value);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) throw std::runtime_error("Cannot create text surface.");
        HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,
            serif ? L"Georgia" : L"Segoe UI");
        auto old_font = SelectObject(dc, font);
        RECT bounds {0, 0, max_width, size * 10};
        DrawTextW(dc, text.c_str(), int(text.size()), &bounds, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        int width = std::max(1L, bounds.right), height = std::max(1L, bounds.bottom);
        BITMAPINFO info {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) { SelectObject(dc, old_font); DeleteObject(font); DeleteDC(dc); throw std::runtime_error("Cannot allocate text surface."); }
        auto old_bitmap = SelectObject(dc, bitmap);
        std::fill_n(static_cast<unsigned*>(pixels), width * height, 0);
        SetTextColor(dc, RGB(255, 255, 255)); SetBkMode(dc, TRANSPARENT);
        RECT area {0, 0, width, height};
        DrawTextW(dc, text.c_str(), int(text.size()), &area, DT_WORDBREAK | DT_NOPREFIX);
        GdiFlush();
        auto* data = static_cast<unsigned*>(pixels);
        for (int i = 0; i < width * height; ++i) { unsigned alpha = data[i] & 255; data[i] = (alpha << 24) | 0x00ffffff; }
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
        if (texture) { SDL_UpdateTexture(texture, nullptr, pixels, width * 4); SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND); }
        SelectObject(dc, old_bitmap); SelectObject(dc, old_font); DeleteObject(bitmap); DeleteObject(font); DeleteDC(dc);
        if (!texture) throw std::runtime_error(SDL_GetError());
        cache.emplace(key, texture);
    } else texture = found->second;
    int width, height; SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b); SDL_SetTextureAlphaMod(texture, color.a);
    SDL_Rect target {x, y, width, height}; SDL_RenderCopy(renderer, texture, nullptr, &target);
}
}
