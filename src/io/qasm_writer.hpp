#pragma once

#include "io/circuit_ir.hpp"
#include <string>

namespace zx {

class QasmWriter {
public:
    static std::string to_string(const CircuitIR& ir);
    static void to_file(const CircuitIR& ir, const std::string& filepath);
};

} // namespace zx
