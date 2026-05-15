#ifndef PIXELCONVERTER_H
#define PIXELCONVERTER_H

#include <cstdint>
#include <tuple>

struct PixelRGB
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

struct PixelLAB
{
    float l;
    float a;
    float b;
};

std::tuple<float, float, float> RGBtoXYZ(uint8_t r, uint8_t g, uint8_t b);
std::tuple<float, float, float> XYZtoLAB(float x, float y, float z);
std::tuple<float, float, float> RGBtoLAB(uint8_t r, uint8_t g, uint8_t b);

std::tuple<float, float, float> LABtoXYZ(float l, float a, float b);
std::tuple<uint8_t, uint8_t, uint8_t> XYZtoRGB(float x, float y, float z);
std::tuple<uint8_t, uint8_t, uint8_t> LABtoRGB(float l, float a, float b);

#endif // PIXELCONVERTER_H