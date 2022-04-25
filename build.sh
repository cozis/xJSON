CC=gcc
FLAGS="-Wall -Wextra -Isrc/"

for arg in "$@"
do
    case $arg in
           --debug) FLAGS="$FLAGS -DDEBUG -g"  ;;
         --release) FLAGS="$FLAGS -DNDEBUG -O3" ;;
        --coverage) FLAGS="$FLAGS -fprofile-arcs -ftest-coverage" ;;
    esac
done

$CC tests/test.c          src/xjson.c -o test       $FLAGS
$CC examples/parse-file.c src/xjson.c -o parse-file $FLAGS