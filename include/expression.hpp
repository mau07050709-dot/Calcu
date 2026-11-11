#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace calcu {
enum class MathError { none, syntax, divideByZero, domain, range, complexity, noAnswer };
enum class NodeKind { literal, name, group, call, prefix, postfix, binary, placeholder };

struct ExpressionNode {
    NodeKind kind;
    uint16_t begin;
    uint16_t end;
    uint16_t marker = 0; // Operador, final del nombre o separador de argumentos.
    int left = -1;
    int right = -1;
    bool closed = true;
};

struct ExpressionTree {
    std::vector<ExpressionNode> nodes;
    int root = -1;
    MathError error = MathError::none;
    bool consumed = false;
};

// Los hijos preceden a sus padres. Los intervalos son posiciones en el texto original.
ExpressionTree parseExpression(std::string_view expression);
}
