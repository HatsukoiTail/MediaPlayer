#ifndef SLIC_H
#define SLIC_H

#include <cstdint>
#include <span>
#include <vector>

std::vector<float> apply_SLIC(std::span<const float> lab_pixels, int width, int height, int scale, int epoch);

#endif // SLIC_H