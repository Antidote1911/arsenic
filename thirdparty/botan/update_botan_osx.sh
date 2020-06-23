#!/bin/bash
set -e
set -x
curl -OL https://github.com/randombit/botan/archive/2.8.0.tar.gz

script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )
botan_archive=2.8.0.tar.gz
botan_extracted_dir_name=botan-2.8.0
botan_parent_dir=$script_dir/

cd $botan_parent_dir

echo "Removing old botan sources..."
rm -rf botan

echo "Extracting new botan sources..."
tar -zxf $botan_archive
cp -r $botan_extracted_dir_name botan
rm -rf $botan_archive $botan_extracted_dir_name
echo "Removing unneeded components..."
cd botan
rm -r doc news.rst
cd src
rm -r build-data/policy/* cli configs fuzzer python scripts tests
echo "Patching..."
# Fix annoying linker warning on macOS
sed -i'' -e 's/all!haiku -> "-pthread"/all!haiku,darwin -> "-pthread"/g' $botan_parent_dir/botan/src/build-data/cc/clang.txt
echo "Done."
