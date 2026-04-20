#!/bin/bash
set -e -x

tmp_dir="${PWD}/tests/tests_data/tmp_analysis"
mkdir -p "$tmp_dir"

g++ -std=c++17 -g -fsanitize=address ./tests/tests_data/leak_example.cpp -o "$tmp_dir/leak_before"
# Отключаем new_delete_type_mismatch, чтобы ASan дошел именно до проверки утечки.
ASAN_OPTIONS=detect_leaks=1:new_delete_type_mismatch=0 "$tmp_dir/leak_before" || true

g++ -std=c++17 -g -fsanitize=address ./tests/tests_data/leak_example_ref.cpp -o "$tmp_dir/leak_after"
ASAN_OPTIONS=detect_leaks=1:new_delete_type_mismatch=0 "$tmp_dir/leak_after" || true
