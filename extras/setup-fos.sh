#!/bin/bash

# Export required variables
export LD_LIBRARY_PATH=~/Projects/OpenStream/extras/fos/install/lib
export FOS_PATH=~/Projects/OpenStream/extras/fos

# Clone the repository
if [ -d "fos" ]
then
  cd fos
  git pull
  cd ..
else
  git clone https://github.com/khoapham/fos.git
fi

# Copy cmake files to fos tree
cp -r fos-cmake/* fos

# Build fos libraries
cd fos
mkdir -p build
cd build
/usr/bin/cmake -DCMAKE_BUILD_TYPE=Debug ..
make install
