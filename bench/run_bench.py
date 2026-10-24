import os
import sys
import time
import json
import csv
import re
import subprocess
from pathlib import Path

# Paths
ROOT_DIR = Path(__file__).resolve().parent.parent
CIRCUITS_DIR = ROOT_DIR / "bench" / "circuits"
RESULTS_DIR = ROOT_DIR / "bench" / "results"
RESULTS_DIR.mkdir(parents=True, exist_ok=True)

ZX_REDUCE_BIN = ROOT_DIR / "build" / "release" / "zx-reduce.exe"
QDD_SIM_BIN = ROOT_DIR.parent / "qdd-sim" / "build" / "release" / "qdd-sim.exe"
QSYN_BIN = ROOT_DIR / "third_party" / "qsyn" / "build" / "qsyn.exe"

# Import PyZX
import pyzx as zx

def run_cmd(cmd_list):
    try:
        res = subprocess.run(cmd_list, capture_output=True, text=True, timeout=60)
        return res.returncode, res.stdout, res.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "Timeout"
    except Exception as e:
        return -2, "", str(e)

def verify_ec(c1_path, c2_path):
    if not QDD_SIM_BIN.exists():
        return "SKIPPED (no qdd-sim)"
    cmd = [str(QDD_SIM_BIN), "--ec", str(c1_path), str(c2_path)]
    ret, out, err = run_cmd(cmd)
    if ret == 0 and "Result: EQUIVALENT" in out:
        return "PASS"
    return "FAIL"

def run_benchmarks():
    circuits = sorted(list(CIRCUITS_DIR.glob("*.qasm")))
    if not circuits:
        print("No benchmark circuits found in", CIRCUITS_DIR)
        return

    results = []
    print(f"Running benchmarks on {len(circuits)} circuits...\n")

    for circ in circuits:
        cname = circ.stem
        print(f"--- Benchmarking {cname} ---")

        # 1. Evaluate with zx-reduce
        out_zx = RESULTS_DIR / f"{cname}_zx_out.qasm"
        stats_zx_path = RESULTS_DIR / f"{cname}_zx_stats.json"

        cmd_zx = [str(ZX_REDUCE_BIN), str(circ), "-o", str(out_zx), "--stats", str(stats_zx_path), "--quiet"]
        ret_zx, out_zx_log, err_zx_log = run_cmd(cmd_zx)

        zx_stats = {}
        if stats_zx_path.exists():
            with open(stats_zx_path) as sf:
                zx_stats = json.load(sf)

        zx_ec = verify_ec(circ, out_zx) if ret_zx == 0 else "FAIL"

        # 2. Evaluate with PyZX
        t_before_pyzx = 0
        t_after_pyzx = 0
        cx_before_pyzx = 0
        cx_after_pyzx = 0
        gates_after_pyzx = 0
        pyzx_time_ms = 0.0
        pyzx_ec = "N/A"
        out_pyzx = RESULTS_DIR / f"{cname}_pyzx_out.qasm"

        try:
            c_pyzx = zx.Circuit.from_qasm_file(str(circ)).to_basic_gates()
            t_before_pyzx = c_pyzx.tcount()
            cx_before_pyzx = sum(1 for g in c_pyzx.gates if g.name in ["CNOT", "CZ"])

            t0 = time.perf_counter()
            g_pyzx = c_pyzx.to_graph()
            zx.full_reduce(g_pyzx)
            c_pyzx_opt = zx.extract_circuit(g_pyzx)
            c_pyzx_opt = zx.basic_optimization(c_pyzx_opt.to_basic_gates())
            pyzx_time_ms = (time.perf_counter() - t0) * 1000.0

            t_after_pyzx = c_pyzx_opt.tcount()
            cx_after_pyzx = sum(1 for g in c_pyzx_opt.gates if g.name in ["CNOT", "CZ"])
            gates_after_pyzx = len(c_pyzx_opt.gates)

            with open(out_pyzx, "w") as pf:
                pf.write(c_pyzx_opt.to_qasm())

            pyzx_ec = verify_ec(circ, out_pyzx)
        except Exception as e:
            print(f"  PyZX error on {cname}: {e}")
            t_after_pyzx = -1
            pyzx_ec = f"ERR: {e}"

        # 3. Evaluate with qsyn
        t_after_qsyn = -1
        cx_after_qsyn = -1
        gates_after_qsyn = -1
        qsyn_time_ms = 0.0
        qsyn_ec = "N/A"
        out_qsyn = RESULTS_DIR / f"{cname}_qsyn_out.qasm"

        if QSYN_BIN.exists():
            try:
                circ_posix = circ.as_posix()
                out_qsyn_posix = out_qsyn.as_posix()
                qsyn_cmd = f"qcir read {circ_posix}; qc2zx; zx optimize -f; zx2qc; qcir print; qcir write {out_qsyn_posix}; quit -f"
                t0_qsyn = time.perf_counter()
                ret_qsyn, out_qsyn_log, err_qsyn_log = run_cmd([str(QSYN_BIN), "-q", "-c", qsyn_cmd])
                qsyn_time_ms = (time.perf_counter() - t0_qsyn) * 1000.0

                if ret_qsyn == 0:
                    match = re.search(r"QCir\s*\(\s*\d+\s*qubits,\s*(\d+)\s*gates,\s*(\d+)\s*2-qubits gates,\s*(\d+)\s*T-gates", out_qsyn_log)
                    if match:
                        gates_after_qsyn = int(match.group(1))
                        cx_after_qsyn = int(match.group(2))
                        t_after_qsyn = int(match.group(3))
                    qsyn_ec = verify_ec(circ, out_qsyn)
                else:
                    qsyn_ec = f"ERR (code {ret_qsyn})"
            except Exception as e:
                print(f"  qsyn error on {cname}: {e}")
                qsyn_ec = f"ERR: {e}"

        zx_ms = zx_stats.get("total_time_ms", 0.0)
        speedup_pyzx = (pyzx_time_ms / zx_ms) if zx_ms > 0 else 0.0
        speedup_qsyn = (qsyn_time_ms / zx_ms) if zx_ms > 0 else 0.0

        record = {
            "circuit": cname,
            "qubits": zx_stats.get("num_qubits", c_pyzx.qubits if 'c_pyzx' in locals() else 0),
            "orig_t": zx_stats.get("t_count_before", t_before_pyzx),
            "orig_cx": zx_stats.get("cnot_count_before", cx_before_pyzx),
            "zx_t": zx_stats.get("t_count_after", -1),
            "zx_cx": zx_stats.get("cnot_count_after", -1),
            "zx_gates": zx_stats.get("total_gates_after", -1),
            "zx_time_ms": zx_ms,
            "zx_ec": zx_ec,
            "pyzx_t": t_after_pyzx,
            "pyzx_cx": cx_after_pyzx if t_after_pyzx >= 0 else -1,
            "pyzx_gates": gates_after_pyzx if t_after_pyzx >= 0 else -1,
            "pyzx_time_ms": pyzx_time_ms,
            "pyzx_ec": pyzx_ec,
            "qsyn_t": t_after_qsyn,
            "qsyn_cx": cx_after_qsyn if t_after_qsyn >= 0 else -1,
            "qsyn_gates": gates_after_qsyn if t_after_qsyn >= 0 else -1,
            "qsyn_time_ms": qsyn_time_ms,
            "qsyn_ec": qsyn_ec,
            "speedup_vs_pyzx": speedup_pyzx,
            "speedup_vs_qsyn": speedup_qsyn
        }
        results.append(record)

        print(f"  zx-reduce : T-count {record['orig_t']} -> {record['zx_t']}, time: {record['zx_time_ms']:.2f} ms [{record['zx_ec']}]")
        print(f"  PyZX      : T-count {record['orig_t']} -> {record['pyzx_t']}, time: {record['pyzx_time_ms']:.2f} ms [{record['pyzx_ec']}]")
        print(f"  qsyn      : T-count {record['orig_t']} -> {record['qsyn_t']}, time: {record['qsyn_time_ms']:.2f} ms [{record['qsyn_ec']}]")
        print(f"  Speedup   : {record['speedup_vs_pyzx']:.2f}x vs PyZX, {record['speedup_vs_qsyn']:.2f}x vs qsyn\n")

    # Save to CSV
    csv_path = RESULTS_DIR / "benchmarks.csv"
    with open(csv_path, "w", newline="") as cf:
        writer = csv.DictWriter(cf, fieldnames=list(results[0].keys()))
        writer.writeheader()
        writer.writerows(results)

    # Save to JSON
    json_path = RESULTS_DIR / "benchmarks.json"
    with open(json_path, "w") as jf:
        json.dump(results, jf, indent=2)

    # Print summary Markdown table
    print("\n" + "="*110)
    print("3-WAY BENCHMARK SUMMARY TABLE: zx-reduce vs PyZX vs qsyn")
    print("="*110)
    print("| Circuit | Qubits | Orig T | zx T | PyZX T | qsyn T | zx (ms) | PyZX (ms) | qsyn (ms) | vs PyZX | vs qsyn | Equiv Check |")
    print("| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |")
    for r in results:
        print(f"| {r['circuit']} | {r['qubits']} | {r['orig_t']} | **{r['zx_t']}** | {r['pyzx_t']} | {r['qsyn_t']} | **{r['zx_time_ms']:.2f}** | {r['pyzx_time_ms']:.2f} | {r['qsyn_time_ms']:.2f} | **{r['speedup_vs_pyzx']:.1f}x** | **{r['speedup_vs_qsyn']:.1f}x** | {r['zx_ec']} / {r['pyzx_ec']} / {r['qsyn_ec']} |")

if __name__ == "__main__":
    run_benchmarks()
