#!/bin/bash
set -euo pipefail

cat > empty.c << 'EOF'
void __mimer_api_stub__() {}
EOF

gcc -O2 -shared -fPIC empty.c -o libmimerapi.so
gcc -O2 -shared -fPIC empty.c -o libodbc.so.2
gcc -O2 -shared -fPIC empty.c -o libqsqlodbc.so
gcc -O2 -shared -fPIC empty.c -o libpq.so.5

rm -f empty.c
