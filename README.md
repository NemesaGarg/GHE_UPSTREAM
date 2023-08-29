# GHE_UPSTREAM
Code for Global Histogram Equalization

Compilation step:
1. gcc -g -c -fPIC -o DisplayPc.o DisplayPc.c
2. gcc -g -c -fPIC -o GHE_Algorithm.o GHE_Algorithm.c
3. gcc -g -shared -o libdpst.so.1 DisplayPc.o GHE_Algorithm.o
