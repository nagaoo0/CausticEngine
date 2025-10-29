#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include <functional>
#include <optional>
#include <array>
#include <vector>
#include <memory>
#include <span>

// Walnut includes
#include "Walnut/Application.h"
#include "Walnut/Layer.h"
#include "Walnut/Image.h"

// GSL-like replacements
namespace gsl {
    template<typename T>
    using span = std::span<T>;
    using czstring = const char*;
    template<typename T>
    using not_null = T;
}