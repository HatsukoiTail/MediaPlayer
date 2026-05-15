#include "PixelConverter.h"

#include <cmath>

std::tuple<float, float, float> RGBtoXYZ(uint8_t r, uint8_t g, uint8_t b)
{
    // normalize RGB values to [0, 1]
    const float fr = r / 255.0;
    const float fg = g / 255.0;
    const float fb = b / 255.0;

    // gamma correction (sRGB -> Linear RGB)
    float lin_r, lin_g, lin_b = 0.0;
    if (fr <= 0.04045)
        lin_r = fr / 12.92;
    else
        lin_r = pow((fr + 0.055) / 1.055, 2.4);
    if (fg <= 0.04045)
        lin_g = fg / 12.92;
    else
        lin_g = pow((fg + 0.055) / 1.055, 2.4);
    if (fb <= 0.04045)
        lin_b = fb / 12.92;
    else
        lin_b = pow((fb + 0.055) / 1.055, 2.4);

    // matrix transformation to XYZ color space
    const float x = lin_r * 0.4124564 + lin_g * 0.3575761 + lin_b * 0.1804375;
    const float y = lin_r * 0.2126729 + lin_g * 0.7151522 + lin_b * 0.0721750;
    const float z = lin_r * 0.0193339 + lin_g * 0.1191920 + lin_b * 0.9503041;

    return {x, y, z};
}

std::tuple<float, float, float> XYZtoLAB(float x, float y, float z)
{
    const float epsilon = 0.008856f;
    const float kappa = 903.3f;

    const float xr = x / 0.950456f;
    const float yr = y / 1.0f;
    const float zr = z / 1.088754f;

    float fx, fy, fz = 0.0;
    if (xr > epsilon)
        fx = pow(xr, 1.0 / 3.0);
    else
        fx = (kappa * xr + 16.0) / 116.0;
    if (yr > epsilon)
        fy = pow(yr, 1.0 / 3.0);
    else
        fy = (kappa * yr + 16.0) / 116.0;
    if (zr > epsilon)
        fz = pow(zr, 1.0 / 3.0);
    else
        fz = (kappa * zr + 16.0) / 116.0;

    float l = 116.0 * fy - 16.0;
    float a = 500.0 * (fx - fy);
    float b = 200.0 * (fy - fz);
    return {l, a, b};
}

std::tuple<float, float, float> RGBtoLAB(uint8_t r, uint8_t g, uint8_t b)
{
    auto [x, y, z] = RGBtoXYZ(r, g, b);
    return XYZtoLAB(x, y, z);
}

std::tuple<float, float, float> LABtoXYZ(float l, float a, float b)
{
    const float fy = (l + 16.0f) / 116.0f;
    const float fx = fy + a / 500.0f;
    const float fz = fy - b / 200.0f;

    constexpr float epsilon = 0.008856f;
    constexpr float kappa_inv = 7.787f;
    constexpr float offset = 16.0f / 116.0f;

    float xr = fx * fx * fx;
    float yr = fy * fy * fy;
    float zr = fz * fz * fz;
    if (xr < epsilon)
    {
        xr = (fx - offset) / kappa_inv;
    }
    if (yr < epsilon)
    {
        yr = (fy - offset) / kappa_inv;
    }
    if (zr < epsilon)
    {
        zr = (fz - offset) / kappa_inv;
    }
    float x = 0.950456f * xr;
    float y = 1.000000f * yr;
    float z = 1.088754f * zr;

    return {x, y, z};
}

std::tuple<uint8_t, uint8_t, uint8_t> XYZtoRGB(float x, float y, float z)
{
    float r = 3.2404542f * x - 1.5371385f * y - 0.4985314f * z;
    float g = -0.9692660f * x + 1.8760108f * y + 0.0415560f * z;
    float b = 0.0556434f * x - 0.2040259f * y + 1.0572252f * z;

    r = std::max(0.0f, std::min(1.0f, r));
    g = std::max(0.0f, std::min(1.0f, g));
    b = std::max(0.0f, std::min(1.0f, b));

    r = r > 0.0031308f ? (1.055f * pow(r, (1.0f / 2.4f)) - 0.055f) : (r * 12.92f);
    g = g > 0.0031308f ? (1.055f * pow(g, (1.0f / 2.4f)) - 0.055f) : (g * 12.92f);
    b = b > 0.0031308f ? (1.055f * pow(b, (1.0f / 2.4f)) - 0.055f) : (b * 12.92f);

    r = std::max(0.0f, std::min(1.0f, r));
    g = std::max(0.0f, std::min(1.0f, g));
    b = std::max(0.0f, std::min(1.0f, b));

    int ir = r * 255.0f + 0.5f;
    int ig = g * 255.0f + 0.5f;
    int ib = b * 255.0f + 0.5f;

    ir = std::max(0, std::min(255, ir));
    ig = std::max(0, std::min(255, ig));
    ib = std::max(0, std::min(255, ib));

    return {static_cast<uint8_t>(ir), static_cast<uint8_t>(ig), static_cast<uint8_t>(ib)};
}

std::tuple<uint8_t, uint8_t, uint8_t> LABtoRGB(float l, float a, float b)
{
    auto [x, y, z] = LABtoXYZ(l, a, b);
    return XYZtoRGB(x, y, z);
}
