#include "ComplexParser.h"
#include <cctype>
#include <cmath>
#include <sstream>
#include <stdexcept>

// --- 各类语法树节点的具体实现 ---
class ConstantNode : public ASTNode {
    std::complex<double> val;
public:
    ConstantNode(double r) : val(r, 0.0) {}
    std::complex<double> evaluate(std::complex<double>) override { return val; }
    std::string toGLSL() override {
        std::ostringstream ss;
        ss << "vec2(" << val.real() << ", 0.0)";
        return ss.str();
    }
};

class VariableNode : public ASTNode {
public:
    std::complex<double> evaluate(std::complex<double> z) override { return z; }
    std::string toGLSL() override { return "z"; }
};

class BinaryOpNode : public ASTNode {
    char op;
    std::shared_ptr<ASTNode> left, right;
public:
    BinaryOpNode(char o, std::shared_ptr<ASTNode> l, std::shared_ptr<ASTNode> r)
        : op(o), left(l), right(r) {
    }

    std::complex<double> evaluate(std::complex<double> z) override {
        auto l = left->evaluate(z);
        auto r = right->evaluate(z);
        if (op == '+') return l + r;
        if (op == '-') return l - r;
        if (op == '*') return l * r;
        if (op == '/') return l / r;
        if (op == '^') return std::pow(l, r.real());
        return 0.0;
    }

    std::string toGLSL() override {
        if (op == '+') return "complex_add(" + left->toGLSL() + ", " + right->toGLSL() + ")";
        if (op == '-') return "complex_sub(" + left->toGLSL() + ", " + right->toGLSL() + ")";
        if (op == '*') return "complex_mul(" + left->toGLSL() + ", " + right->toGLSL() + ")";
        if (op == '/') return "complex_div(" + left->toGLSL() + ", " + right->toGLSL() + ")";
        if (op == '^') return "complex_pow(" + left->toGLSL() + ", float(" + right->toGLSL() + ".x))";
        return "vec2(0.0)";
    }
};

class UnaryFuncNode : public ASTNode {
    std::string name;
    std::shared_ptr<ASTNode> child;
public:
    UnaryFuncNode(std::string n, std::shared_ptr<ASTNode> c) : name(n), child(c) {}

    std::complex<double> evaluate(std::complex<double> z) override {
        auto v = child->evaluate(z);
        // C++ 标准库 <complex> 完整支持所有初等复数运算
        if (name == "sin")   return std::sin(v);
        if (name == "cos")   return std::cos(v);
        if (name == "tan")   return std::tan(v);
        if (name == "sinh")  return std::sinh(v);
        if (name == "cosh")  return std::cosh(v);
        if (name == "tanh")  return std::tanh(v);
        if (name == "asin")  return std::asin(v);
        if (name == "acos")  return std::acos(v);
        if (name == "atan")  return std::atan(v);
        if (name == "exp")   return std::exp(v);
        if (name == "log" || name == "ln") return std::log(v);
        if (name == "log10") return std::log10(v);
        if (name == "sqrt")  return std::sqrt(v);
        return 0.0;
    }

    std::string toGLSL() override {
        if (name == "ln") return "complex_log(" + child->toGLSL() + ")";
        return "complex_" + name + "(" + child->toGLSL() + ")";
    }
};

// --- 解析器核心控制逻辑 ---
ComplexParser::ComplexParser() : m_root(nullptr), m_pos(0) {}

char ComplexParser::peek() {
    if (m_pos < m_expr.length()) return m_expr[m_pos];
    return '\0';
}

char ComplexParser::get() {
    if (m_pos < m_expr.length()) return m_expr[m_pos++];
    return '\0';
}

void ComplexParser::skipWhitespace() {
    while (peek() == ' ' || peek() == '\t') m_pos++;
}

bool ComplexParser::parse(const std::string& expression) {
    m_expr = expression;
    m_pos = 0;
    try {
        m_root = parseExpression();
        skipWhitespace();
        if (peek() != '\0') return false;
        return m_root != nullptr;
    }
    catch (...) {
        return false;
    }
}

std::complex<double> ComplexParser::evaluate(std::complex<double> z) {
    if (m_root) return m_root->evaluate(z);
    return 0.0;
}

std::string ComplexParser::generateGLSL() {
    if (m_root) return m_root->toGLSL();
    return "vec2(0.0)";
}

std::shared_ptr<ASTNode> ComplexParser::parseExpression() {
    auto token = parseTerm();
    while (true) {
        skipWhitespace();
        char op = peek();
        if (op == '+' || op == '-') {
            get();
            auto nextTerm = parseTerm();
            token = std::make_shared<BinaryOpNode>(op, token, nextTerm);
        }
        else {
            break;
        }
    }
    return token;
}

std::shared_ptr<ASTNode> ComplexParser::parseTerm() {
    auto token = parsePower();
    while (true) {
        skipWhitespace();
        char op = peek();
        if (op == '*' || op == '/') {
            get();
            auto nextPower = parsePower();
            token = std::make_shared<BinaryOpNode>(op, token, nextPower);
        }
        else {
            break;
        }
    }
    return token;
}

std::shared_ptr<ASTNode> ComplexParser::parsePower() {
    auto token = parseFactor();
    skipWhitespace();
    if (peek() == '^') {
        get();
        auto exponent = parsePower();
        token = std::make_shared<BinaryOpNode>('^', token, exponent);
    }
    return token;
}

std::shared_ptr<ASTNode> ComplexParser::parseFactor() {
    skipWhitespace();
    char c = peek();
    if (c == '(') {
        get();
        auto node = parseExpression();
        skipWhitespace();
        if (get() != ')') throw std::runtime_error("括号不匹配");
        return node;
    }
    if (c == 'z' || c == 'Z') {
        get();
        return std::make_shared<VariableNode>();
    }
    if (std::isdigit(c) || c == '.') {
        std::string s;
        while (std::isdigit(peek()) || peek() == '.') {
            s += get();
        }
        return std::make_shared<ConstantNode>(std::stod(s));
    }
    if (std::isalpha(c)) {
        std::string funcName;
        while (std::isalpha(peek()) || std::isdigit(peek())) {
            funcName += get();
        }
        skipWhitespace();
        if (peek() == '(') {
            get();
            auto child = parseExpression();
            if (get() != ')') throw std::runtime_error("函数括号不匹配");
            if (funcName == "sin" || funcName == "cos" || funcName == "tan" ||
                funcName == "sinh" || funcName == "cosh" || funcName == "tanh" ||
                funcName == "asin" || funcName == "acos" || funcName == "atan" ||
                funcName == "exp" || funcName == "log" || funcName == "ln" ||
                funcName == "log10" || funcName == "sqrt") {
                return std::make_shared<UnaryFuncNode>(funcName, child);
            }
        }
    }
    throw std::runtime_error("未知语法错误");
}