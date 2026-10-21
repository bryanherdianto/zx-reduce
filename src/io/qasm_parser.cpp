#include "io/qasm_parser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <numbers>
#include <cmath>

namespace zx {

QasmParser::QasmParser(std::string_view source) {
    QasmLexer lexer(source);
    tokens = lexer.tokenize_all();
}

CircuitIR QasmParser::from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open QASM file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_string(buffer.str());
}

CircuitIR QasmParser::from_string(std::string_view source) {
    QasmParser parser(source);
    return parser.parse();
}

const Token& QasmParser::peek() const {
    if (pos >= tokens.size()) return tokens.back();
    return tokens[pos];
}

const Token& QasmParser::get() {
    if (pos >= tokens.size()) return tokens.back();
    return tokens[pos++];
}

bool QasmParser::match(TokenType type) {
    if (peek().type == type) {
        get();
        return true;
    }
    return false;
}

const Token& QasmParser::expect(TokenType type, std::string_view err_msg) {
    if (peek().type != type) {
        throw std::runtime_error("Parser error at line " + std::to_string(peek().line) +
                                 ", col " + std::to_string(peek().col) + ": " + std::string(err_msg) +
                                 ", got '" + peek().text + "'");
    }
    return get();
}

uint32_t QasmParser::parse_qubit() {
    auto id_tok = expect(TokenType::Identifier, "Expected register name");
    expect(TokenType::LBracket, "Expected '['");
    auto num_tok = expect(TokenType::Number, "Expected qubit index");
    expect(TokenType::RBracket, "Expected ']'");

    uint32_t idx = static_cast<uint32_t>(std::stoul(num_tok.text));
    auto it = reg_offsets.find(id_tok.text);
    if (it == reg_offsets.end()) {
        throw std::runtime_error("Undeclared quantum register: " + id_tok.text);
    }
    return it->second + idx;
}

Phase QasmParser::parse_phase() {
    // Parse expressions like pi, pi/2, 3*pi/4, -pi/4, or float
    bool negative = false;
    if (peek().type == TokenType::Minus) {
        get();
        negative = true;
    } else if (peek().type == TokenType::Plus) {
        get();
    }

    int64_t num = 1;
    int64_t den = 1;

    if (peek().type == TokenType::Number) {
        auto num_tok = get();
        if (num_tok.text.find('.') != std::string::npos) {
            double val = std::stod(num_tok.text);
            if (negative) val = -val;
            double in_pi = val;
            if (match(TokenType::Star)) {
                expect(TokenType::Pi, "Expected 'pi' after '*'");
            } else {
                in_pi = val / std::numbers::pi;
            }
            if (match(TokenType::Slash)) {
                auto den_tok = expect(TokenType::Number, "Expected denominator");
                in_pi /= std::stod(den_tok.text);
            }
            for (int64_t d : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}) {
                double d_dbl = static_cast<double>(d);
                double rounded = std::round(in_pi * d_dbl);
                if (std::abs(in_pi * d_dbl - rounded) < 1e-4) {
                    return Phase(static_cast<int64_t>(rounded), d);
                }
            }
            return Phase(static_cast<int64_t>(std::round(in_pi * 1024.0)), 1024);
        }

        num = std::stoll(num_tok.text);
        if (match(TokenType::Star)) {
            expect(TokenType::Pi, "Expected 'pi' after '*'");
        }
    } else if (peek().type == TokenType::Pi) {
        get();
        num = 1;
    }

    if (match(TokenType::Slash)) {
        auto den_tok = expect(TokenType::Number, "Expected denominator");
        den = std::stoll(den_tok.text);
    }

    if (negative) {
        num = -num;
    }
    return Phase(num, den);
}

CircuitIR QasmParser::parse() {
    CircuitIR ir;

    while (peek().type != TokenType::Eof) {
        const auto& tok = peek();

        if (tok.type == TokenType::Identifier) {
            std::string name = tok.text;

            if (name == "OPENQASM" || name == "include") {
                // Skip statement
                while (peek().type != TokenType::Semicolon && peek().type != TokenType::Eof) {
                    get();
                }
                match(TokenType::Semicolon);
            } else if (name == "qreg") {
                get();
                auto reg_name = expect(TokenType::Identifier, "Expected qreg name").text;
                expect(TokenType::LBracket, "Expected '['");
                uint32_t size = static_cast<uint32_t>(std::stoul(expect(TokenType::Number, "Expected qreg size").text));
                expect(TokenType::RBracket, "Expected ']'");
                expect(TokenType::Semicolon, "Expected ';'");

                reg_offsets[reg_name] = total_qubits;
                total_qubits += size;
                ir.num_qubits = total_qubits;
            } else if (name == "creg") {
                // Ignore creg declarations
                while (peek().type != TokenType::Semicolon && peek().type != TokenType::Eof) {
                    get();
                }
                match(TokenType::Semicolon);
            } else if (name == "measure" || name == "reset" || name == "if") {
                throw std::runtime_error("Unsupported classical/measurement operation: " + name);
            } else if (name == "gate") {
                // Skip composite gate definitions
                get();
                while (peek().type != TokenType::LBrace && peek().type != TokenType::Eof) {
                    get();
                }
                if (match(TokenType::LBrace)) {
                    int depth = 1;
                    while (depth > 0 && peek().type != TokenType::Eof) {
                        if (peek().type == TokenType::LBrace) depth++;
                        else if (peek().type == TokenType::RBrace) depth--;
                        get();
                    }
                }
            } else if (name == "barrier") {
                get();
                while (peek().type != TokenType::Semicolon && peek().type != TokenType::Eof) {
                    get();
                }
                match(TokenType::Semicolon);
            } else {
                // Gate call
                get();
                Phase phase = Phase::zero();

                if (match(TokenType::LParen)) {
                    phase = parse_phase();
                    // If multiple parameters e.g. u3(theta, phi, lambda), parse rest
                    while (match(TokenType::Comma)) {
                        parse_phase();
                    }
                    expect(TokenType::RParen, "Expected ')'");
                }

                std::vector<uint32_t> qargs;
                qargs.push_back(parse_qubit());
                while (match(TokenType::Comma)) {
                    qargs.push_back(parse_qubit());
                }
                expect(TokenType::Semicolon, "Expected ';'");

                // Map gate name to IR
                if (name == "h") {
                    ir.add_gate(Gate::make_h(qargs[0]));
                } else if (name == "x") {
                    ir.add_gate(Gate::make_x(qargs[0]));
                } else if (name == "y") {
                    ir.add_gate(Gate::make_y(qargs[0]));
                } else if (name == "z") {
                    ir.add_gate(Gate::make_z(qargs[0]));
                } else if (name == "s") {
                    ir.add_gate(Gate::make_s(qargs[0]));
                } else if (name == "sdg") {
                    ir.add_gate(Gate::make_sdg(qargs[0]));
                } else if (name == "t") {
                    ir.add_gate(Gate::make_t(qargs[0]));
                } else if (name == "tdg") {
                    ir.add_gate(Gate::make_tdg(qargs[0]));
                } else if (name == "rz" || name == "p" || name == "u1") {
                    ir.add_gate(Gate::make_rz(qargs[0], phase));
                } else if (name == "rx") {
                    ir.add_gate(Gate::make_rx(qargs[0], phase));
                } else if (name == "cx" || name == "cnot") {
                    ir.add_gate(Gate::make_cx(qargs[0], qargs[1]));
                } else if (name == "cz") {
                    ir.add_gate(Gate::make_cz(qargs[0], qargs[1]));
                } else if (name == "cp" || name == "cphase") {
                    Phase half_phase(phase.num, phase.den * 2);
                    ir.add_gate(Gate::make_rz(qargs[0], half_phase));
                    ir.add_gate(Gate::make_rz(qargs[1], half_phase));
                    ir.add_gate(Gate::make_cx(qargs[0], qargs[1]));
                    ir.add_gate(Gate::make_rz(qargs[1], -half_phase));
                    ir.add_gate(Gate::make_cx(qargs[0], qargs[1]));
                } else if (name == "swap") {
                    ir.add_gate(Gate::make_swap(qargs[0], qargs[1]));
                } else if (name == "ccx" || name == "toffoli") {
                    ir.add_gate(Gate::make_ccx(qargs[0], qargs[1], qargs[2]));
                } else if (name == "ccz") {
                    // ccz: H on target, then ccx, then H on target
                    ir.add_gate(Gate::make_h(qargs[2]));
                    ir.add_gate(Gate::make_ccx(qargs[0], qargs[1], qargs[2]));
                    ir.add_gate(Gate::make_h(qargs[2]));
                }
            }
        } else {
            get();
        }
    }

    return ir;
}

} // namespace zx
