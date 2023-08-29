# GHE_UPSTREAM
Code for Global Histogram Equalization

Compilation step:
1. gcc -g -c -fPIC -o DisplayPc.o DisplayPc.c
2. gcc -g -c -fPIC -o DisplayDpstAlgorithm7_x.o DisplayDpstAlgorithm7_x.c
3. gcc -g -shared -o libdpst.so.1 DisplayPcDpst.o DisplayDpstAlgorithm7_x.o
