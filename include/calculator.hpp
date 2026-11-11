#pragma once

#include "expression.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace calcu {
enum class Angle { degrees, radians };
enum class ResultFormat { decimal, fraction, scientific };

struct Fraction {
    int64_t numerator;
    int64_t denominator;
};

struct Evaluation {
    double value = 0;
    MathError error = MathError::none;
    std::optional<Fraction> exact{};
};

Evaluation evaluate(std::string_view expression, Angle angle, const Evaluation* answer = nullptr);
std::string formatResult(const Evaluation& result, ResultFormat format);
const char* errorLabel(MathError error);
}
