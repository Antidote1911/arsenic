#!/bin/bash

set -o errexit -o nounset

# Update platform
echo "Updating platform..."
sudo -E apt-get -yq --no-install-suggests --no-install-recommends --force-yes install p7zip-full
sudo -E apt-get -yq --no-install-suggests --no-install-recommends --force-yes install chrpath

# Hold on to current directory
project_dir=$(pwd)
qt_install_dir=/opt

# Get Qt
echo "Installing Qt..."
cd ${qt_install_dir}
echo "Downloading Qt files..."
sudo wget https://github.com/adolby/qt-more-builds/releases/download/5.7/qt-opensource-5.7.0-linux-x86_64.7z
echo "Extracting Qt files..."
sudo 7z x qt-opensource-5.7.0-linux-x86_64.7z &> /dev/null

# Install Qt Installer Framework
echo "Installing Qt Installer Framework..."
sudo wget https://github.com/adolby/qt-more-builds/releases/download/qt-ifw-2.0.3/qt-installer-framework-opensource-2.0.3-linux.7z
sudo 7z x qt-installer-framework-opensource-2.0.3-linux.7z &> /dev/null

# Add Qt binaries to path
echo "Adding Qt binaries to path..."
PATH=${qt_install_dir}/Qt/5.7/gcc_64/bin/:${qt_install_dir}/Qt/QtIFW2.0.3/bin/:${PATH}

# Get Botan
# echo "Installing Botan..."
# sudo wget https://github.com/randombit/botan/archive/1.11.32.zip
# sudo 7z x 1.11.32.zip &>/dev/null
# sudo chmod -R +x /usr/local/botan-1.11.32/
# cd /usr/local/botan-1.11.32/
# ./configure.py --cc=clang --amalgamation --disable-shared --with-zlib
# cp botan_all_aesni.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_aesni.cpp
# cp botan_all_avx2.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_avx2.cpp
# cp botan_all_internal.h ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_internal.h
# cp botan_all_rdrand.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_rdrand.cpp
# cp botan_all_rdseed.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_rdseed.cpp
# cp botan_all_ssse3.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all_ssse3.cpp
# cp botan_all.cpp ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all.cpp
# cp botan_all.h ${project_dir}/src/cryptography/botan/linux/gcc/x86_64/botan_all.h

# Build Arsenic
echo "Building Arsenic..."
cd ${project_dir}
qmake -v
qmake -config release -spec linux-g++-64
make

echo "Done!"

exit 0 
