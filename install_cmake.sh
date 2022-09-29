version=3.19
build=1
## don't modify from here
curr_dir=$PWD
mkdir ~/temp
cd ~/temp
wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
tar -xzvf cmake-$version.$build.tar.gz
cd cmake-$version.$build/

./bootstrap
make -j$(nproc)
make install
camke --version
cd $curr_dir
