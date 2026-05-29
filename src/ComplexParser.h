#pragma once
#include <complex>
#include <string>
#include <memory>
#include <vector>

// 抽象语法树（AST）基础节点
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::complex<double> evaluate(std::complex<double> z) = 0; // CPU求值
    virtual std::string toGLSL() = 0;                                  // 翻译为GPU代码
};

// 核心解析器类
class ComplexParser {
public:
    ComplexParser();
    ~ComplexParser() = default;

    bool parse(const std::string& expression);
    
    std::complex<double> evaluate(std::complex<double> z);
    
    std::string generateGLSL();

private:
    std::shared_ptr<ASTNode> m_root;
    std::string m_expr;
    size_t m_pos;

    char peek();
    char get();
    void skipWhitespace();
    
    std::shared_ptr<ASTNode> parseExpression();
    std::shared_ptr<ASTNode> parseTerm();
    std::shared_ptr<ASTNode> parseFactor();
    std::shared_ptr<ASTNode> parsePower();
};