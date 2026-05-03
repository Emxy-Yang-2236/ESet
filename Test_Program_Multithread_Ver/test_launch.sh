#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# ESet Subtask2 test runner
# Place this script inside: Test_Program_Multithread_Ver/
# Expected project layout:
#
#   project_root/
#   ├── src.hpp
#   └── Test_Program_Multithread_Ver/
#       ├── correctness_test.cpp
#       ├── benchmark_mixed.cpp
#       ├── benchmark_single_op.cpp
#       ├── instrumented_benchmark.cpp
#       └── run_all_tests.sh
#
# Output layout:
#
#   Test_Program_Multithread_Ver/ltest/
#   ├── bin/              executable files
#   ├── correctness/      correctness output
#   ├── mixed/            mixed benchmark csv
#   ├── single_op/        single-op benchmark csv
#   ├── instrumented/     instrumented benchmark csv
#   ├── gperftools_cpu/   optional gperftools CPU profile outputs
#   └── logs/             compile/run logs
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC_HPP="$PROJECT_ROOT/src.hpp"

LTEST_DIR="$SCRIPT_DIR/ltest"
BIN_DIR="$LTEST_DIR/bin"
LOG_DIR="$LTEST_DIR/logs"
CORRECT_DIR="$LTEST_DIR/correctness"
MIXED_DIR="$LTEST_DIR/mixed"
SINGLE_DIR="$LTEST_DIR/single_op"
INSTR_DIR="$LTEST_DIR/instrumented"
GTOOLS_DIR="$LTEST_DIR/gperftools_cpu"


CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -DNDEBUG -pthread}"
PROFILE_CXXFLAGS="${PROFILE_CXXFLAGS:--std=c++17 -O2 -DNDEBUG -g -fno-omit-frame-pointer -rdynamic -pthread}"

JOBS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

CORRECT_OPS="${CORRECT_OPS:-300000}"
CORRECT_KEY_RANGE="${CORRECT_KEY_RANGE:-600000}"
CORRECT_SEED="${CORRECT_SEED:-123456789}"

N_MIXED="${N_MIXED:-500000}"
REPEAT_MIXED="${REPEAT_MIXED:-8}"

N_SINGLE="${N_SINGLE:-200000}"
REPEAT_SINGLE="${REPEAT_SINGLE:-8}"

N_INSTRUMENTED="${N_INSTRUMENTED:-200000}"

RUN_GPERFTOOLS="${RUN_GPERFTOOLS:-auto}"

mkdir -p "$BIN_DIR" "$LOG_DIR" "$CORRECT_DIR" "$MIXED_DIR" "$SINGLE_DIR" "$INSTR_DIR" "$GTOOLS_DIR"

log() {
  printf '\n[%s] %s\n' "$(date '+%F %T')" "$*"
}

require_file() {
  if [[ ! -f "$1" ]]; then
    echo "ERROR: required file not found: $1" >&2
    exit 1
  fi
}

compile_one() {
  local src="$1"
  local out="$2"
  local flags="$3"

  require_file "$SCRIPT_DIR/$src"
  log "Compiling $src -> $out"
  "$CXX" $flags -I"$PROJECT_ROOT" "$SCRIPT_DIR/$src" -o "$out" \
    2>&1 | tee "$LOG_DIR/compile_${src%.cpp}.log"
}

run_and_save() {
  local name="$1"
  local outfile="$2"
  shift 2

  log "Running $name"
  log "Output: $outfile"
  "$@" > "$outfile" 2> "$LOG_DIR/${name}.stderr.log"
  log "Finished $name"
}

print_config() {
  cat > "$LTEST_DIR/run_config.txt" <<CONFIG
Run time:             $(date '+%F %T')
Script dir:           $SCRIPT_DIR
Project root:         $PROJECT_ROOT
Compiler:             $($CXX --version | head -n 1)
CXXFLAGS:             $CXXFLAGS
PROFILE_CXXFLAGS:     $PROFILE_CXXFLAGS
Jobs:                 $JOBS
Correctness ops:      $CORRECT_OPS
Correctness key range:$CORRECT_KEY_RANGE
Correctness seed:     $CORRECT_SEED
Mixed N:              $N_MIXED
Mixed repeat:         $REPEAT_MIXED
Single-op N:          $N_SINGLE
Single-op repeat:     $REPEAT_SINGLE
Instrumented N:       $N_INSTRUMENTED
CONFIG
}

# -------------------------
# Sanity checks
# -------------------------
require_file "$SRC_HPP"
print_config
log "Configuration saved to $LTEST_DIR/run_config.txt"

# -------------------------
# Compile normal benchmark programs
# -------------------------
compile_one "correctness_test.cpp" "$BIN_DIR/correctness_test" "$CXXFLAGS"
compile_one "benchmark_mixed.cpp" "$BIN_DIR/benchmark_mixed" "$CXXFLAGS"
compile_one "benchmark_single_op.cpp" "$BIN_DIR/benchmark_single_op" "$CXXFLAGS"
compile_one "instrumented_benchmark.cpp" "$BIN_DIR/instrumented_benchmark" "$CXXFLAGS"

# -------------------------
# Run correctness test
# -------------------------
run_and_save "correctness_test" "$CORRECT_DIR/correctness.txt" \
  "$BIN_DIR/correctness_test" \
  --ops "$CORRECT_OPS" \
  --key-range "$CORRECT_KEY_RANGE" \
  --seed "$CORRECT_SEED" \
  --jobs 16

# -------------------------
# Run mixed benchmark
# -------------------------
run_and_save "benchmark_mixed_both" "$MIXED_DIR/mixed_both.csv" \
  "$BIN_DIR/benchmark_mixed" \
  --n "$N_MIXED" \
  --repeat "$REPEAT_MIXED" \
  --jobs 24 \
  --container both


# -------------------------
# Run single operation benchmark
# -------------------------
run_and_save "benchmark_single_op_both" "$SINGLE_DIR/single_op_both.csv" \
  "$BIN_DIR/benchmark_single_op" \
  --n "$N_SINGLE" \
  --repeat "$REPEAT_SINGLE" \
  --jobs 24 \
  --container both


run_and_save "instrumented_both" "$INSTR_DIR/instrumented_both.csv" \
  "$BIN_DIR/instrumented_benchmark" \
  --n "$N_INSTRUMENTED" \
  --container both

# -------------------------
# Optional gperftools CPU profiling
# It is only used to identify hot functions;
# -------------------------
find_profiler_lib() {
  ldconfig -p 2>/dev/null | awk '/libprofiler\.so/{print $NF; exit}'
}

PROFILER_LIB="$(find_profiler_lib || true)"
PPROF_BIN="$(command -v google-pprof || command -v pprof || true)"

DO_GPERFTOOLS=0
if [[ "$RUN_GPERFTOOLS" == "1" ]]; then
  DO_GPERFTOOLS=1
elif [[ "$RUN_GPERFTOOLS" == "auto" && -n "$PROFILER_LIB" && -n "$PPROF_BIN" ]]; then
  DO_GPERFTOOLS=1
fi

if [[ "$DO_GPERFTOOLS" == "1" ]]; then
  if [[ -z "$PROFILER_LIB" || -z "$PPROF_BIN" ]]; then
    log "Skipping gperftools CPU profiling: libprofiler or pprof tool not found."
  else
    log "Compiling profiling build for benchmark_mixed"
    compile_one "benchmark_mixed.cpp" "$BIN_DIR/benchmark_mixed_profile" "$PROFILE_CXXFLAGS"

    log "Running gperftools CPU profile for ESet mixed benchmark"
    CPUPROFILE="$GTOOLS_DIR/mixed_eset.prof" \
    LD_PRELOAD="$PROFILER_LIB" \
      "$BIN_DIR/benchmark_mixed_profile" \
      --n "$N_MIXED" --repeat 1 --jobs 1 --container eset \
      > "$GTOOLS_DIR/mixed_eset_profile_run.csv" \
      2> "$LOG_DIR/gperftools_eset.stderr.log"

    log "Running gperftools CPU profile for std::set mixed benchmark"
    CPUPROFILE="$GTOOLS_DIR/mixed_stl.prof" \
    LD_PRELOAD="$PROFILER_LIB" \
      "$BIN_DIR/benchmark_mixed_profile" \
      --n "$N_MIXED" --repeat 1 --jobs 1 --container stl \
      > "$GTOOLS_DIR/mixed_stl_profile_run.csv" \
      2> "$LOG_DIR/gperftools_stl.stderr.log"

    log "Generating text CPU profile reports"
    "$PPROF_BIN" --text "$BIN_DIR/benchmark_mixed_profile" "$GTOOLS_DIR/mixed_eset.prof" \
      > "$GTOOLS_DIR/mixed_eset_cpu.txt" || true
    "$PPROF_BIN" --text "$BIN_DIR/benchmark_mixed_profile" "$GTOOLS_DIR/mixed_stl.prof" \
      > "$GTOOLS_DIR/mixed_stl_cpu.txt" || true

    log "gperftools CPU profiling outputs saved to $GTOOLS_DIR"
  fi
else
  log "Skipping gperftools CPU profiling. Set RUN_GPERFTOOLS=1 to force it."
fi

# -------------------------
# Summary
# -------------------------
cat <<SUMMARY

============================================================
All requested non-perf tests finished.

Output root:
  $LTEST_DIR

Important files:
  $CORRECT_DIR/correctness.txt
  $MIXED_DIR/mixed_both.csv
  $SINGLE_DIR/single_op_both.csv
  $INSTR_DIR/instrumented_both.csv

Optional profiling files, if generated:
  $GTOOLS_DIR/mixed_eset_cpu.txt
  $GTOOLS_DIR/mixed_stl_cpu.txt

Configuration:
  $LTEST_DIR/run_config.txt

Logs:
  $LOG_DIR
============================================================
SUMMARY
