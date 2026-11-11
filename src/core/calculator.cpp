#include "calculator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <vector>

namespace calcu {
namespace {
using Integer = int64_t;
using Exact = std::optional<Fraction>;

constexpr double pi = 3.14159265358979323846;
constexpr double e = 2.71828182845904523536;
constexpr Integer limit = std::numeric_limits<Integer>::max();

Integer magnitude(Integer value) { return value < 0 ? -value : value; }

bool multiply(Integer a, Integer b, Integer& product) {
    if (b != 0 && magnitude(a) > limit / magnitude(b)) return false;
    product = a * b;
    return true;
}

Fraction reduced(Integer numerator, Integer denominator) {
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    const Integer divisor = std::gcd(numerator, denominator);
    return {numerator / divisor, denominator / divisor};
}

Exact multiply(Fraction a, Fraction b) {
    const auto left = reduced(a.numerator, b.denominator);
    const auto right = reduced(b.numerator, a.denominator);
    Fraction result{};
    if (!multiply(left.numerator, right.numerator, result.numerator) ||
        !multiply(left.denominator, right.denominator, result.denominator)) return {};
    return result;
}

Exact add(Fraction a, Fraction b) {
    const auto common = std::gcd(a.denominator, b.denominator);
    Integer x = 0, y = 0;
    if (!multiply(a.numerator, b.denominator / common, x) ||
        !multiply(b.numerator, a.denominator / common, y)) return {};
    if ((y > 0 && x > limit - y) || (y < 0 && x < -limit - y)) return {};
    const Integer sum = x + y;
    const auto divisor = std::gcd(sum, common);
    Integer denominator = 0;
    if (!multiply(a.denominator / common, b.denominator / divisor, denominator)) return {};
    return Fraction{sum / divisor, denominator};
}

Exact power(Fraction base, Integer exponent) {
    if (exponent < 0) base = reduced(base.denominator, base.numerator);
    Exact result = Fraction{1, 1};
    for (auto n = magnitude(exponent); n > 0; n /= 2) {
        if (n % 2 != 0) result = multiply(*result, base);
        if (!result) return {};
        if (n >= 2) {
            const auto squared = multiply(base, base);
            if (!squared) return {};
            base = *squared;
        }
    }
    return result;
}

std::optional<Integer> perfectRoot(Integer value, Integer index) {
    if (value <= 1 || index == 1) return value;
    if (index >= 63) return {};
    Integer low = 2;
    Integer high = value / 2 + 1;
    while (low <= high) {
        const auto candidate = low + (high - low) / 2;
        Integer product = 1;
        for (Integer i = 0; i < index && product <= value; ++i) {
            if (product > value / candidate) {
                product = 0;
                break;
            }
            product *= candidate;
        }
        if (product == value) return candidate;
        if (product == 0) high = candidate - 1;
        else low = candidate + 1;
    }
    return {};
}

Exact literalFraction(std::string_view token) {
    const auto eAt = token.find_first_of("eE");
    const auto mantissa = token.substr(0, eAt);
    if (mantissa.find_first_of("123456789") == std::string_view::npos) return Fraction{0, 1};
    const auto dot = mantissa.find('.');
    int scale = dot == std::string_view::npos ? 0 : -static_cast<int>(mantissa.size() - dot - 1);
    if (eAt != std::string_view::npos) {
        const auto exponent = token.substr(eAt + 1);
        const bool negative = exponent.front() == '-';
        int digits = 0;
        for (char ch : exponent) {
            if (ch == '+' || ch == '-') continue;
            digits = digits * 10 + (ch - '0');
            if (digits > 512) return {};
        }
        scale += negative ? -digits : digits;
    }

    std::string digits;
    for (char ch : mantissa) if (ch != '.') digits.push_back(ch);
    while (!digits.empty() && digits.back() == '0') {
        ++scale;
        digits.pop_back();
    }
    if (scale < -62 || scale > 18) return {};

    // Cancelar factores de 10 antes de acumular el numerador de 64 bits.
    Integer denominator = 1;
    for (int factor : {2, 5}) {
        for (int i = 0; i < -scale; ++i) {
            if ((digits.back() - '0') % factor == 0) {
                int carry = 0;
                for (char& ch : digits) {
                    const int value = 10 * carry + (ch - '0');
                    ch = static_cast<char>('0' + value / factor);
                    carry = value % factor;
                }
            } else if (!multiply(denominator, factor, denominator)) {
                return {};
            }
        }
    }

    Integer numerator = 0;
    for (char ch : digits) {
        const auto digit = ch - '0';
        if (numerator > (limit - digit) / 10) return {};
        numerator = numerator * 10 + digit;
    }
    while (scale > 0) {
        if (!multiply(numerator, 10, numerator)) return {};
        --scale;
    }
    return reduced(numerator, denominator);
}

Evaluation failure(MathError error) { return {0, error}; }

Evaluation checked(double value, Exact exact = {}, bool mustBeNonzero = false) {
    if (exact) {
        return {static_cast<double>(exact->numerator) / static_cast<double>(exact->denominator),
                MathError::none, exact};
    }
    if (std::isnan(value)) return failure(MathError::domain);
    if (!std::isfinite(value) || (mustBeNonzero && value == 0)) return failure(MathError::range);
    return {value, MathError::none};
}

Evaluation integerValue(Integer value) { return checked(0, Fraction{value, 1}); }

std::optional<Integer> integer(const Evaluation& value) {
    if (value.exact && value.exact->denominator == 1) return value.exact->numerator;
    return {};
}

Evaluation literal(std::string_view token) {
    const auto exact = literalFraction(token);
    const std::string decimal(token);
    const auto mantissa = token.substr(0, token.find_first_of("eE"));
    const bool nonzero = mantissa.find_first_of("123456789") != std::string_view::npos;
    return checked(std::strtod(decimal.c_str(), nullptr), exact, nonzero);
}

Evaluation negate(Evaluation input) {
    input.value = -input.value;
    if (input.exact) input.exact->numerator = -input.exact->numerator;
    return input;
}

Evaluation binary(char op, const Evaluation& a, const Evaluation& b) {
    if (a.error != MathError::none) return a;
    if (b.error != MathError::none) return b;
    const auto x = a.value, y = b.value;
    const bool exact = a.exact && b.exact;
    switch (op) {
        case '+': return checked(x + y, exact ? add(*a.exact, *b.exact) : Exact{});
        case '-': return checked(x - y, exact ? add(*a.exact, *negate(b).exact) : Exact{});
        case '*': return checked(x * y, exact ? multiply(*a.exact, *b.exact) : Exact{}, x != 0 && y != 0);
        case '/':
            if (y == 0) return failure(MathError::divideByZero);
            return checked(x / y, exact ? multiply(*a.exact, reduced(b.exact->denominator, b.exact->numerator))
                                        : Exact{}, x != 0);
        case '^': {
            if (x == 0 && y < 0) return failure(MathError::divideByZero);
            if (x == 0 && y == 0) return failure(MathError::domain);
            const auto exponent = integer(b);
            if (x < 0 && !exponent) return failure(MathError::domain);
            double approximate = std::pow(std::abs(x), y);
            if (x < 0 && *exponent % 2 != 0) approximate = -approximate;
            return checked(approximate, a.exact && exponent ? power(*a.exact, *exponent) : Exact{}, x != 0);
        }
        default: return failure(MathError::syntax);
    }
}

Evaluation factorial(const Evaluation& value) {
    const auto n = integer(value);
    if (!n || *n < 0) return failure(MathError::domain);
    if (*n > 170) return failure(MathError::range);
    Evaluation result = integerValue(1);
    for (Integer i = 2; i <= *n; ++i) result = binary('*', result, integerValue(i));
    return result;
}

Evaluation arrangements(const Evaluation& first, const Evaluation& second, bool combinations) {
    const auto n = integer(first), k = integer(second);
    if (!n || !k || *n < 0 || *k < 0 || *k > *n) return failure(MathError::domain);
    const auto count = combinations ? std::min(*k, *n - *k) : *k;
    Evaluation result = integerValue(1);
    for (Integer i = 1; i <= count; ++i) {
        const auto term = *n - i + 1;
        const auto factor = combinations ? reduced(term, i) : Fraction{term, 1};
        const auto exact = result.exact ? multiply(*result.exact, factor) : Exact{};
        // Divide primero para evitar desbordar un resultado intermedio innecesario.
        result = checked((result.value / static_cast<double>(factor.denominator)) *
                         static_cast<double>(factor.numerator), exact);
        if (result.error != MathError::none) break;
    }
    return result;
}

Evaluation root(const Evaluation& radicand, const Evaluation& degree) {
    const auto index = integer(degree);
    if (degree.value == 0) return failure(MathError::domain);
    if (radicand.value == 0 && degree.value < 0) return failure(MathError::divideByZero);
    const bool negative = radicand.value < 0;
    if (negative && (!index || *index % 2 == 0)) return failure(MathError::domain);
    Exact exact;
    if (index && radicand.exact) {
        const auto numerator = perfectRoot(magnitude(radicand.exact->numerator), magnitude(*index));
        const auto denominator = perfectRoot(radicand.exact->denominator, magnitude(*index));
        if (numerator && denominator) {
            const auto signedNumerator = negative ? -*numerator : *numerator;
            exact = *index > 0 ? reduced(signedNumerator, *denominator)
                               : reduced(*denominator, signedNumerator);
        }
    }
    const auto approximation = std::pow(std::abs(radicand.value), 1.0 / degree.value);
    return checked(negative ? -approximation : approximation, exact, radicand.value != 0);
}

double naturalLog(const Evaluation& value) {
    if (value.exact && value.value >= 0.5 && value.value <= 2) {
        const auto a = value.exact->numerator, b = value.exact->denominator;
        return std::log1p(static_cast<double>(a - b) / static_cast<double>(b));
    }
    return std::log(value.value);
}

Evaluation logarithm(const Evaluation& x, const Evaluation* base) {
    if (x.value <= 0 || (base && base->value <= 0)) return failure(MathError::domain);
    if (!base) return checked(naturalLog(x), x.exact && x.exact->numerator == x.exact->denominator
        ? Exact{Fraction{0, 1}} : Exact{});

    const auto divisor = naturalLog(*base);
    if (divisor == 0) return failure(MathError::domain);
    const auto approximate = naturalLog(x) / divisor;
    if (x.exact && base->exact && std::abs(approximate) <= 64) {
        const auto exponent = static_cast<Integer>(std::round(approximate));
        const auto exact = power(*base->exact, exponent);
        if (exact && exact->numerator == x.exact->numerator && exact->denominator == x.exact->denominator) {
            return integerValue(exponent);
        }
    }
    return checked(approximate);
}

Evaluation function(std::string_view name, const Evaluation& first, const Evaluation* second, Angle angle) {
    if (first.error != MathError::none) return first;
    if (second && second->error != MathError::none) return *second;
    const auto x = first.value;
    if (second) {
        if (name == "root") return root(first, *second);
        if (name == "ncr" || name == "npr") return arrangements(first, *second, name == "ncr");
        if (name == "log" || name == "logb") return logarithm(first, second);
        return failure(MathError::syntax);
    }
    if (name == "sqrt" || name == "cbrt") return root(first, integerValue(name == "sqrt" ? 2 : 3));
    if (name == "ln") return logarithm(first, nullptr);
    if (name == "log") {
        const auto base = integerValue(10);
        return logarithm(first, &base);
    }

    const auto degrees = angle == Angle::degrees ? std::remainder(x, 360.0) : 0;
    const auto radians = angle == Angle::degrees ? degrees * (pi / 180.0) : x;
    if (name == "sin") {
        if (first.exact && first.exact->numerator == 0) return integerValue(0);
        return checked(std::sin(radians));
    }
    if (name == "cos") {
        if (first.exact && first.exact->numerator == 0) return integerValue(1);
        return checked(std::cos(radians));
    }
    if (name == "tan") {
        if (angle == Angle::degrees ? std::abs(degrees) == 90 :
            std::abs(std::remainder(x, pi)) == pi / 2) return failure(MathError::domain);
        if (first.exact && first.exact->numerator == 0) return integerValue(0);
        return checked(std::tan(radians));
    }
    const double scale = angle == Angle::degrees ? 180.0 / pi : 1.0;
    if (name == "asin" || name == "acos") {
        if (x < -1 || x > 1) return failure(MathError::domain);
        return checked((name == "asin" ? std::asin(x) : std::acos(x)) * scale);
    }
    if (name == "atan") return checked(std::atan(x) * scale);
    return failure(MathError::syntax);
}
}

Evaluation evaluate(std::string_view expression, Angle angle, const Evaluation* answer) {
    const auto tree = parseExpression(expression);
    if (tree.error != MathError::none) return failure(tree.error);

    std::vector<Evaluation> values;
    values.reserve(tree.nodes.size());
    for (const auto& node : tree.nodes) {
        const auto text = expression.substr(node.begin, node.end - node.begin);
        Evaluation result = failure(MathError::syntax);
        switch (node.kind) {
            case NodeKind::literal: result = literal(text); break;
            case NodeKind::name:
                if (text == "pi") result = checked(pi);
                if (text == "e") result = checked(e);
                if (text == "ans" || text == "Ans") result = answer ? *answer : failure(MathError::noAnswer);
                break;
            case NodeKind::group: result = values[node.left]; break;
            case NodeKind::prefix:
                result = values[node.left];
                if (expression[node.marker] == '-') result = negate(result);
                break;
            case NodeKind::postfix: {
                const auto& input = values[node.left];
                if (input.error != MathError::none) result = input;
                else if (expression[node.marker] == '!') result = factorial(input);
                else result = binary('/', input, integerValue(100));
                break;
            }
            case NodeKind::binary:
                result = binary(expression[node.marker], values[node.left], values[node.right]);
                break;
            case NodeKind::call:
                result = function(expression.substr(node.begin, node.marker - node.begin),
                    values[node.left], node.right < 0 ? nullptr : &values[node.right], angle);
                break;
            case NodeKind::placeholder: break;
        }
        values.push_back(result);
    }
    return values[tree.root];
}

std::string formatResult(const Evaluation& result, ResultFormat format) {
    if (result.error != MathError::none) return errorLabel(result.error);
    if (format == ResultFormat::fraction && result.exact) {
        std::string text = std::to_string(result.exact->numerator);
        if (result.exact->denominator != 1) text += "/" + std::to_string(result.exact->denominator);
        return text;
    }
    char buffer[40]{};
    const auto value = result.value == 0 ? 0 : result.value;
    std::snprintf(buffer, sizeof buffer, format == ResultFormat::scientific ? "%.10e" : "%.12g", value);
    return buffer;
}

const char* errorLabel(MathError error) {
    switch (error) {
        case MathError::none: return "";
        case MathError::syntax: return "Error de sintaxis";
        case MathError::divideByZero: return "Division por cero";
        case MathError::domain: return "Fuera de dominio";
        case MathError::range: return "Fuera de rango";
        case MathError::complexity: return "Expresion compleja";
        case MathError::noAnswer: return "Ans sin resultado";
    }
    return "Error";
}
}
