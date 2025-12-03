echo esharedhash.c:
gcc -pthread -Wall esharedhash.c -o b
time ./b pi.txt -t

echo sharedhash.c:
gcc -pthread -Wall sharedhash.c -o a
time ./a pi.txt -t

rm a b
