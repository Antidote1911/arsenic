#!/bin/sh
#test $# -eq 1 || { echo "This script needs a Botan archive as its sole argument."; exit 1; }
##https://github.com/randombit/botan/archive/2.19.1.tar.gz
set -e
set -x

wget https://github.com/randombit/botan/archive/2.19.1.tar.gz
script_dir=$(readlink -f $(dirname $0))
botan_archive=2.19.1.tar.gz
botan_extracted_dir_name=botan-2.19.1
botan_parent_dir=$script_dir/

cd $botan_parent_dir

echo "Removing old botan sources..."
rm -rf botan

echo "Extracting new botan sources..."
tar xf $botan_archive
cp -r $botan_extracted_dir_name botan
rm -rf $botan_archive $botan_extracted_dir_name
echo "Removing unneeded components..."
cd botan
cd src
echo "Patching..."
# Fix annoying linker warning on macOS
sed -i 's/all!haiku -> "-pthread"/all!haiku,darwin -> "-pthread"/g' \
    "$botan_parent_dir/botan/src/build-data/cc/clang.txt"

echo "Done."
