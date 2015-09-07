#!/bin/sh

RELEASE=3.5.2

VERSION=$(date +%Y%m%d_%H%M)
mkdir compute_clang_${VERSION}
cd compute_clang_${VERSION}

mkdir bin
cp ../build/Release/bin/clang bin/
cp ../build/Release/bin/llc bin/
cp ../build/Release/bin/llvm-dis bin/
cp ../build/Release/bin/llvm-as bin/
cp ../build/Release/bin/spir-encoder bin/
cp ../build/Release/bin/spir-verifier bin/
cp ../build/Release/bin/applecl-encoder bin/

cp -R ../build/Release/lib/clang/${RELEASE}/include clang

cp -R ../libcxx/include libcxx
cp ../libcxx/{LICENSE.TXT,CREDITS.TXT} libcxx/

cd ..

zip -r compute_clang_${VERSION}.zip compute_clang_${VERSION}
