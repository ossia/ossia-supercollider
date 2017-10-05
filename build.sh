#!/bin/bash -eux
 export PATH=$PATH:/usr/local/bin

 if [[ ! -d "supercollider" ]]; then
     echo "cloning supercollider repository..."
     git clone --recursive https://github.com/supercollider/supercollider
 else
     (
     echo "supercollider already downloaded..."
     echo "checking for updates..."
     cd supercollider
     git pull
     git submodule update --init --recursive
     )
 fi

 if [[ ! -d "libossia" ]]; then
     echo "now cloning libossia"
  git clone --recursive https://github.com/OSSIA/libossia
else
  (
  echo "libossia already downloaded..."
  echo "checking for updates..."
  cd libossia
  git pull
  git submodule update --init --recursive
  )
fi

mkdir -p build
(
cd build
export OS_IS_LINUX=0
export OS_IS_OSX=0
if [[ -d "/proc" ]]; then
  export OS_IS_LINUX=1
else
  export OS_IS_OSX=1

  # Set-up OS X dependencies
  export HOMEBREW_BIN=$(command -v brew)
  if [[ "$HOMEBREW_BIN" == "" ]]; then
    echo "Homebrew is not installed."
    echo "Please install it by running the following command:"
    echo '    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"'
    exit 1
  fi

  if brew ls --versions qt@5.5 > /dev/null; then
      echo "qt@5.5 with brew correctly installed"
  else
      echo "installing qt@5.5 with homebrew"
      brew install qt@5.5
  fi

  if brew ls --versions boost > /dev/null; then
      echo "boost already installed"
  else
      echo "installing boost with homebrew"
      brew install boost
  fi

  export CMAKE_BIN=$(command -v cmake)
  if [[ "$CMAKE_BIN" == "" ]]; then
    brew install cmake
  fi
fi  

# CMake and build libossia
echo "now building libossia..."
cmake ../libossia -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=ossia-inst -DOSSIA_PYTHON=0 -DOSSIA_NO_QT=1 -DOSSIA_TESTING=0 -DOSSIA_STATIC=1 -DOSSIA_NO_SONAME=1 -DOSSIA_PD=0 -DBOOST_ROOT=/usr/local/opt/boost/include -DOSSIA_NO_QT=1
make -j8
echo "libossia built succesfully... installing"
make install

# Cleaning up
rm -rf libossia build
)

cd supercollider
git checkout 3.9 # always latest stable version

cd SCClassLibrary
if [[ ! -d "Ossia" ]]; then
    mkdir Ossia
fi

cd ../..

# Copy OssiaClassLibrary and overwrite some sc files
yes | cp -rf Ossia/Overwrites/root/CMakeLists.txt supercollider
yes | cp -rf Ossia/Overwrites/lang/CMakeLists.txt supercollider/lang
yes | cp -rf Ossia/Overwrites/lang/PyrPrimitive.cpp supercollider/lang/LangPrimSource
yes | cp -rf Ossia/Classes/ossia.sc supercollider/SCClassLibrary/Ossia
yes | cp -rf Ossia_light supercollider/SCClassLibrary
yes | cp -rf Ossia/HelpSource/Guides/OssiaReference.schelp supercollider/HelpSource/Guides
yes | cp -rf Ossia/HelpSource/Classes supercollider/HelpSource/Classes
yes | cp -rf Ossia/HelpSource/Help.schelp supercollider/HelpSource

shopt -s dotglob nullglob
mv supercollider/HelpSource/Classes/Classes supercollider/HelpSource/Classes/Ossia
mv supercollider/HelpSource/Classes/Ossia/* supercollider/HelpSource/Classes/
rm -rf supercollider/HelpSource/Classes/Ossia

cd build/ossia-inst/include
if [[ ! -d "ossia-sc" ]]; then
    mkdir ossia-sc
fi

cd ../../..

# move the ossia prim header into ossia include directory... 
# the header should maybe be present in libossia repository...
yes | cp -rf Ossia/Primitives/pyrossiaprim.h build/ossia-inst/include/ossia-sc

# build supercollider
cd supercollider

mkdir -p build
cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5.5 -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=../../build -DSYSTEM_BOOST=ON -DBoost_INCLUDE_DIR=/usr/local/opt/boost/include -DBoost_DIR=/usr/local/opt/boost
make -j8
make install


