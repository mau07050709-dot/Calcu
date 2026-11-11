#include "expression.hpp"
#include "config.hpp"

#include <utility>

namespace calcu {
namespace {
bool digit(char ch) { return ch >= '0' && ch <= '9'; }
bool letter(char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'); }

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    ExpressionTree parse() {
        tree_.nodes.reserve(input_.size() + 1);
        tree_.root = sum(0);
        space();
        tree_.consumed = position_ == input_.size();
        if (!tree_.consumed) fail(MathError::syntax);
        return std::move(tree_);
    }

private:
    char peek() const { return position_ < input_.size() ? input_[position_] : '\0'; }

    void space() {
        while (peek() == ' ' || peek() == '\t') ++position_;
    }

    bool take(char ch) {
        space();
        if (peek() != ch) return false;
        ++position_;
        return true;
    }

    void fail(MathError error) {
        if (tree_.error == MathError::none || error == MathError::complexity) tree_.error = error;
    }

    int node(NodeKind kind, uint16_t begin, uint16_t end, uint16_t marker = 0,
             int left = -1, int right = -1, bool closed = true) {
        tree_.nodes.push_back({kind, begin, end, marker, left, right, closed});
        return static_cast<int>(tree_.nodes.size() - 1);
    }

    int missing(MathError error = MathError::syntax) {
        fail(error);
        return node(NodeKind::placeholder, position_, position_);
    }

    int binary(int left, uint16_t marker, int right) {
        return node(NodeKind::binary, tree_.nodes[left].begin, tree_.nodes[right].end,
                    marker, left, right);
    }

    int sum(unsigned depth) {
        int left = product(depth);
        while (take('+') || take('-')) {
            const auto marker = static_cast<uint16_t>(position_ - 1);
            left = binary(left, marker, product(depth));
        }
        return left;
    }

    int product(unsigned depth) {
        int left = unary(depth);
        while (take('*') || take('/')) {
            const auto marker = static_cast<uint16_t>(position_ - 1);
            left = binary(left, marker, unary(depth));
        }
        return left;
    }

    int unary(unsigned depth) {
        if (depth > 24) return missing(MathError::complexity);
        if (take('+') || take('-')) {
            const auto marker = static_cast<uint16_t>(position_ - 1);
            const int child = unary(depth + 1);
            return node(NodeKind::prefix, marker, tree_.nodes[child].end, marker, child);
        }
        int left = atom(depth);
        while (take('%') || take('!')) {
            const auto marker = static_cast<uint16_t>(position_ - 1);
            left = node(NodeKind::postfix, tree_.nodes[left].begin, position_, marker, left);
        }
        if (!take('^')) return left;
        const auto marker = static_cast<uint16_t>(position_ - 1);
        return binary(left, marker, unary(depth + 1));
    }

    int atom(unsigned depth) {
        if (take('(')) {
            const auto begin = static_cast<uint16_t>(position_ - 1);
            const int child = sum(depth + 1);
            const bool closed = take(')');
            if (!closed) fail(MathError::syntax);
            return node(NodeKind::group, begin, position_, begin, child, -1, closed);
        }

        const auto begin = position_;
        while (letter(peek())) ++position_;
        if (begin == position_) return number();
        const auto nameEnd = position_;
        if (!take('(')) return node(NodeKind::name, begin, nameEnd);

        const int first = sum(depth + 1);
        int second = -1;
        if (take(',')) second = sum(depth + 1);
        const bool closed = take(')');
        if (!closed) fail(MathError::syntax);
        return node(NodeKind::call, begin, position_, nameEnd, first, second, closed);
    }

    int number() {
        const auto begin = position_;
        while (digit(peek())) ++position_;
        bool hasDigits = begin != position_;
        if (peek() == '.') {
            ++position_;
            const auto start = position_;
            while (digit(peek())) ++position_;
            hasDigits |= position_ != start;
        }
        if (position_ == begin) return missing();
        if (!hasDigits) fail(MathError::syntax);
        if (peek() == 'e' || peek() == 'E') {
            ++position_;
            if (peek() == '-' || peek() == '+') ++position_;
            const auto start = position_;
            while (digit(peek())) ++position_;
            if (start == position_) fail(MathError::syntax);
        }
        return node(NodeKind::literal, begin, position_);
    }

    std::string_view input_;
    uint16_t position_ = 0;
    ExpressionTree tree_;
};
}

ExpressionTree parseExpression(std::string_view expression) {
    if (expression.size() > config::expressionLimit) return {{}, -1, MathError::complexity};
    return Parser(expression).parse();
}
}
