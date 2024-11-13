gcc src/main.c -o emu -O2 -s -Isrc/lib/sokol -pthread -lGL -ldl -lm -lX11 -lasound -lXi -lXcursor
