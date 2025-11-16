#!/bin/bash

cat > empty.c << 'EOF'
void __mimer_api_stub__() {}
EOF

# gcc -O2 -shared -fPIC empty.c -o stublib.so
#ln -s ./stublib.so libmimerapi.so
#ln -s ./stublib.so libodbc.so.2
#ln -s ./stublib.so libqsqlodbc.so
#ln -s ./stublib.so libpq.so.5

gcc -O2 -shared -fPIC empty.c -o libmimerapi.so
gcc -O2 -shared -fPIC empty.c -o libodbc.so.2
gcc -O2 -shared -fPIC empty.c -o libqsqlodbc.so
gcc -O2 -shared -fPIC empty.c -o libpq.so.5
