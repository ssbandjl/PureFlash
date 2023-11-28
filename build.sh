clear
echo "start build"
cd /home/xb/project/PureFlash
rm -rf build_deb

export PUREFLASH_HOME=`pwd`

mkdir build_deb; cd build_deb
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=`which ninja` ..
ninja
