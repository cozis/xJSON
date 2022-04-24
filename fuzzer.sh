afl-clang-fast fuzzer.c xjson.c -o fuzzer
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
export AFL_SKIP_CPUFREQ=1
afl-fuzz -i samples/ -o out -m none -d -- ./fuzzer