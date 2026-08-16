#pragma once

#include <cstddef>
#include <string>

namespace gbtest {

std::string sha256_hex(const void* data, std::size_t size);

}  // namespace gbtest
