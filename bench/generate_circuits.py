import os
import shutil
import pyzx as zx

out_dir = "bench/circuits"
os.makedirs(out_dir, exist_ok=True)

# 1. Copy benchmark circuits from qsyn
qsyn_dir = "third_party/qsyn/benchmark/qasm"
qsyn_files = ["tof2.qasm", "tof3.qasm", "tof2tof2.qasm", "achilles.qasm", "spidernest_4_0.qasm"]
for f in qsyn_files:
    src = os.path.join(qsyn_dir, f)
    if os.path.exists(src):
        shutil.copy(src, os.path.join(out_dir, f))
        print(f"Copied {f}")

# 2. Barenco Toffoli 3-qubit
barenco_tof_3 = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
h q[2];
cx q[1],q[2];
tdg q[2];
cx q[0],q[2];
t q[2];
cx q[1],q[2];
tdg q[2];
cx q[0],q[2];
t q[1];
t q[2];
h q[2];
cx q[0],q[1];
t q[0];
tdg q[1];
cx q[0],q[1];
"""
with open(os.path.join(out_dir, "barenco_tof_3.qasm"), "w") as f:
    f.write(barenco_tof_3)
print("Generated barenco_tof_3.qasm")

# 3. mod5_4 (4-qubit arithmetic circuit)
mod5_4 = """OPENQASM 2.0;
include "qelib1.inc";
qreg q[5];
cx q[3],q[4];
cx q[0],q[3];
cx q[2],q[4];
cx q[1],q[3];
cx q[0],q[2];
cx q[1],q[2];
ccx q[0],q[1],q[2];
ccx q[2],q[3],q[4];
"""
with open(os.path.join(out_dir, "mod5_4.qasm"), "w") as f:
    f.write(mod5_4)
print("Generated mod5_4.qasm")

# 4. QFT 4-qubit
c_qft = zx.generate.qft(4)
with open(os.path.join(out_dir, "qft_4.qasm"), "w") as f:
    f.write(c_qft.to_qasm())
print("Generated qft_4.qasm")

# 5. Random Clifford+T 5-qubit
import random
random.seed(42)
c_rand = zx.generate.CNOT_HAD_PHASE_circuit(5, 40)
with open(os.path.join(out_dir, "random_clifford_t_5q.qasm"), "w") as f:
    f.write(c_rand.to_qasm())
print("Generated random_clifford_t_5q.qasm")

print("All benchmark circuits ready!")
