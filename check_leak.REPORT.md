Прогон с утечкой:
```
g++ -std=c++17 -g -fsanitize=address ./tests/tests_data/leak_example.cpp -o /x/tests/tests_data/tmp_analysis/leak_before
```
```
ASAN_OPTIONS=detect_leaks=1:new_delete_type_mismatch=0
```
```
=================================================================
==65==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 4000 byte(s) in 1 object(s) allocated from:
    #0 0xffffae6784a8 in operator new[](unsigned long) ../../../../src/libsanitizer/asan/asan_new_delete.cproot@08b953653e54:/x# ./check_leak.sh
+ tmp_dir=/x/tests/tests_data/tmp_analysis
+ mkdir -p /x/tests/tests_data/tmp_analysis
+ g++ -std=c++17 -g -fsanitize=address ./tests/tests_data/leak_example.cpp -o /x/tests/tests_data/tmp_analysis/leak_before
+ ASAN_OPTIONS=detect_leaks=1:new_delete_type_mismatch=0
+ /x/tests/tests_data/tmp_analysis/leak_before

=================================================================
==81==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 4000 byte(s) in 1 object(s) allocated from:
    #0 0xffffb6c484a8 in operator new[](unsigned long) ../../../../src/libsanitizer/asan/asan_new_delete.cpp:98
    #1 0xaaaae5220e58 in Derived::Derived() tests/tests_data/leak_example.cpp:10
    #2 0xaaaae5220d7c in main tests/tests_data/leak_example.cpp:17
    #3 0xffffb66484c0  (/lib/aarch64-linux-gnu/libc.so.6+0x284c0) (BuildId: d5ef86dde36cbd3289566cf5098226035d76f2e1)
    #4 0xffffb6648594 in __libc_start_main (/lib/aarch64-linux-gnu/libc.so.6+0x28594) (BuildId: d5ef86dde36cbd3289566cf5098226035d76f2e1)
    #5 0xaaaae5220c6c in _start (/x/tests/tests_data/tmp_analysis/leak_before+0xc6c) (BuildId: 90e94acf016008ab1ed1a856cbac37c3b08ddc8f)

SUMMARY: AddressSanitizer: 4000 byte(s) leaked in 1 allocation(s).
```

Прогон с исправленной утечкой:
```
g++ -std=c++17 -g -fsanitize=address ./tests/tests_data/leak_example_ref.cpp -o /x/tests/tests_data/tmp_analysis/leak_after
```
```
ASAN_OPTIONS=detect_leaks=1:new_delete_type_mismatch=0
```
```
```
