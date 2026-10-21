#pragma once

#include "io/circuit_ir.hpp"
#include "io/qasm_lexer.hpp"
#include <string_view>
#include <unordered_map>

namespace zx {

class QasmParser {
public:
    explicit QasmParser(std::string_view source);
    static CircuitIR from_file(const std::string& filepath);
    static CircuitIR from_string(std::string_view source);

    CircuitIR parse();

private:
    std::vector<Token> tokens;
    size_t pos = 0;

    std::unordered_map<std::string, uint32_t> reg_offsets;
    uint32_t total_qubits = 0;

    const Token& peek() const;
    const Token& get();
    bool match(TokenType type);
    const Token& expect(TokenType type, std::string_view err_msg);

    uint32_t parse_qubit();
    Phase parse_phase();
};

} // namespace zx
