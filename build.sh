#!/bin/bash -eux

export OS_IS_LINUX=0
export OS_IS_OSX=0
export BOOST_ROOT=""
export BOOST_INCLUDE=""
export QT_PATH=""
export DISTRO=""

# PLATFORM ----------------------------------------------------------------------------

UNAME=$(uname | tr "[:upper:]" "[:lower:]")
# If Linux, try to determine specific distribution
if [ "$UNAME" == "linux" ]; then
    # If available, use LSB to identify distribution
    if [ -f /etc/lsb-release -o -d /etc/lsb-release.d ]; then
        DISTRO=$(lsb_release -i | cut -d: -f2 | sed s/'^\t'//)
    # Otherwise, use release info file
    else
        DISTRO=$(ls -d /etc/[A-Za-z]*[_-][rv]e[lr]* | grep -v "lsb" | cut -d'/' -f3 | cut -d'-' -f1 | cut -d'_' -f1)
    fi
fi
# For everything else (or if above failed), just use generic identifier
[ "$DISTRO" == "" ] && DISTRO=$UNAME
unset UNAME

# DEPENDENCIES ------------------------------------------------------------------------

if [ "$DISTRO" = "darwin" ]; then

  # check homebrew installation
  export HOMEBREW_BIN=$(command -v brew)
  if [[ "$HOMEBREW_BIN" == "" ]]; then
    echo "Homebrew is not installed."
    echo "Please install it by running the following command:"
    echo '    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"'
    exit 1
  fi

  # CHECK HOMEBREW DEPENDENCIES

    # QT 5.5 (required for supercollider's webkit)
  if brew ls --versions qt@5.5 > /dev/null; then
      echo "qt@5.5 with brew correctly installed"
  else
      echo "installing qt@5.5 with homebrew"
      brew install qt@5.5
  fi

    # BOOST >= 1.65
  if brew ls --versions boost > /dev/null; then
      echo "boost already installed"
  else
      echo "installing boost with homebrew"
      brew install boost
  fi

    # CMAKE
    if brew ls --versions cmake > /dev/null; then
      echo "cmake already installed"
  else
      echo "installing cmake with homebrew"
      brew install cmake
  fi

  # ADDING DEFAULT PATHS
  BOOST_ROOT="/usr/local/opt/boost"
  BOOST_INCLUDE="/usr/local/opt/boost/include"
  QT_PATH="/usr/local/opt/qt@5.5"

elif [ "$DISTRO" = "ubuntu" ]; then

  # checking/installing ossia & supercollider dependencies
  sudo apt update
  sudo apt-get install build-essential libjack-dev libsndfile1-dev libxt-dev libfftw3-dev libudev-dev \
  qt5-default qt5-qmake qttools5-dev qttools5-dev-tools qtdeclarative5-dev libqt5webkit5-dev \
  qtpositioning5-dev libqt5sensors5-dev libqt5opengl5-dev \
  libavahi-compat-libdnssd-dev git wget 

  sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y 
  sudo add-apt-repository ppa:beineri/opt-qt591-trusty -y
  sudo apt update
  sudo apt -y install g++-7 qt-latest

  # installing cmake
  wget https://cmake.org/files/v3.9/cmake-3.9.1-Linux-x86_64.tar.gz
  tar xaf cmake-3.9.1-Linux-x86_64.tar.gz

  # installing boost
  wget http://downloads.sourceforge.net/project/boost/boost/1.65.0/boost_1_65_0.tar.bz2
  tar xaf boost_1_65_0.tar.bz2

  BOOST_ROOT=$(pwd)/boost_1_65_0
  BOOST_INCLUDE=$(pwd)/boost_1_65_0/include
  QT_PATH=/opt/qt59/lib/cmake/Qt5

elif [ "$DISTRO" = "archlinux" ]; then
  sudo pacman -S git cmake 

  BOOST_ROOT="/usr/lib"
  BOOST_INCLUDE="/usr/include"

fi

# CLONING REPOSITORIES ------------------------------------------------------------------------

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

# LIBOSSIA BUILD ------------------------------------------------------------------------------

mkdir -p build

(

  cd build

  # CMake and build libossia
  echo "now building libossia..."
  cmake ../libossia -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=ossia-inst -DOSSIA_PYTHON=0 -DOSSIA_NO_QT=1 -DOSSIA_TESTING=0 -DOSSIA_STATIC=1 -DOSSIA_NO_SONAME=1 -DOSSIA_PD=0 -DBOOST_ROOT=$BOOST_ROOT
  make -j8
  echo "libossia built succesfully... installing"
  make install

  # Cleaning up
  rm -rf libossia build

)

# COPY & OVERWRITES ------------------------------------------------------------------------

cd supercollider
git checkout 3.9

cd SCClassLibrary
if [[ ! -d "Ossia" ]]; then
    mkdir Ossia
fi

cd ../..

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

# SUPERCOLLIDER BUILD ----------------------------------------------------------------------

cd supercollider

# remove packaged boost, which gets somehow included even with SYSTEM_BOOST=ON
rm -rf external_libraries/boost

mkdir -p build
cd build

if [ "$DISTRO" = "darwin" ]; then 
  cmake .. -DCMAKE_PREFIX_PATH=$QT_PATH -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../../build -DSYSTEM_BOOST=ON -DBoost_INCLUDE_DIR=$BOOST_INCLUDE -DBoost_DIR=$BOOST_ROOT

else 
  cmake .. -DCMAKE_PREFIX_PATH=$QT_PATH -DCMAKE_BUILD_TYPE=Release -DSYSTEM_BOOST=ON -DBoost_INCLUDE_DIR=$BOOST_INCLUDE -DBoost_DIR=$BOOST_ROOT
fi


make -j8
sudo make install


