#!/bin/sh

VERSION=$(date +%Y%m%d_%H%M)
mkdir compute_clang_${VERSION}
cd compute_clang_${VERSION}
cp ../build/Release/bin/clang++ compute_clang
cp ../build/Release/bin/llc compute_llc
cp ../build/Release/bin/llvm-dis compute_dis
cp ../build/Release/bin/llvm-as compute_as
cp ../build/Release/bin/spir-encoder .
cp ../build/Release/bin/applecl-encoder .

mkdir libcxx
cp -R ../libcxx/clang libcxx/
cp -R ../libcxx/include libcxx/
cp ../libcxx/{LICENSE.TXT,CREDITS.TXT} libcxx/

cd ..

zip -r compute_clang_${VERSION}.zip compute_clang_${VERSION}
