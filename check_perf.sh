#!/bin/bash
set -e -x

tmp_dir="${PWD}/tests/tests_data/tmp_analysis"
mkdir -p "$tmp_dir"

g++ -std=c++17 -g ./tests/tests_data/perf_example.cpp -o "$tmp_dir/perf_before"

g++ -std=c++17 -g ./tests/tests_data/perf_example_ref.cpp -o "$tmp_dir/perf_after"
