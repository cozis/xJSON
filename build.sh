CC=gcc
FLAGS="-Wall -Wextra"

for arg in "$@"
do
    case $arg in
           --debug) FLAGS="$FLAGS -DDEBUG -g" 
                    ;;
         --release) FLAGS="$FLAGS -DNDEBUG -O3" 
                    ;;
        --coverage) FLAGS="$FLAGS -fprofile-arcs -ftest-coverage"
                    ;;
    esac
done

$CC       test.c xjson.c -o test       $FLAGS
$CC parse-file.c xjson.c -o parse-file $FLAGS