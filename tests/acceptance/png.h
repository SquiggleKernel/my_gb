#pragma once

#include <cstdint>
#include <string>

namespace gbtest {

// 8-bit greyscale PNG. Uses stored (uncompressed) deflate blocks so there is no
// zlib dependency; these images only ever get looked at by a human.
bool write_grey_png(const std::string& path, const std::uint8_t* pixels, int width, int height);

}  // namespace gbtest
