  #!/bin/bash -eux

  export OS_IS_LINUX=0
  export OS_IS_OSX=0
  export BOOST_ROOT=""
  export BOOST_INCLUDE=""
  export BOOST_LIBS=""
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

    # CHECK HOMEBREW DEPENDENCIE
    
    if [ "$1" != "offline" ]; then

      # QT 5.5 (required for supercollider's webkit
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

    fi

    # ADDING DEFAULT PATHS
    BOOST_ROOT="/usr/local/opt/boost"
    BOOST_INCLUDE="/usr/local/opt/boost/include"
    QT_PATH="/usr/local/opt/qt@5.5"

  elif [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "elementary" ] ; then

    # checking/installing ossia & supercollider dependencies
    
    if [ "$1" != "offline" ]; then
        #sudo add-apt-repository ppa:beineri/opt-qt591-trusty
        #sudo apt update
        sudo apt-get install libjack-dev libsndfile1-dev libxt-dev libfftw3-dev libudev-dev \
        qt5-default qt5-qmake qttools5-dev qttools5-dev-tools qtdeclarative5-dev libqt5webkit5-dev \
        qtpositioning5-dev libqt5sensors5-dev libqt5opengl5-dev \
        libavahi-compat-libdnssd-dev git wget 
        sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y 
        #sudo apt update
        sudo apt-get install g++-7 qt-latest
    fi

    # installing non-packaged dependencies
    
    (
      if [ ! -d "dependencies" ]; then
  	    mkdir dependencies
      fi

      cd dependencies

    # cmake

    if [[ ! -d "cmake-3.9.3-Linux-x86_64" ]]; then
    	wget https://cmake.org/files/v3.9/cmake-3.9.3-Linux-x86_64.tar.gz
    	tar xaf cmake-3.9.3-Linux-x86_64.tar.gz
  	rm -rf cmake-3.9.3-Linux-x86_64.tar.gz
    fi

    # download and install boost

    if [[ ! -d "boost_1_65_1" ]]; then	
  	wget http://downloads.sourceforge.net/project/boost/boost/1.65.1/boost_1_65_1.tar.bz2
    	tar xaf boost_1_65_1.tar.bz2
    fi

    if [[ ! -d "boost" ]]; then
  	( 
  	mkdir boost
  	cd boost_1_65_1
  	 ./bootstrap.sh --with-libraries=atomic,date_time,chrono,exception,timer,thread,system,filesystem,program_options,regex,test --prefix=../boost
  	 ./b2 install
  	rm -rf ../boost_1_65_1.tar.bz2
  	)
    fi

    )
    BOOST_ROOT="$(pwd)/dependencies/boost" 
    BOOST_LIBS="$(pwd)/dependencies/boost/lib"
    BOOST_INCLUDE="$(pwd)/dependencies/boost/include"
    QT_PATH="/opt/qt59/lib/cmake/Qt5"

  elif [ "$DISTRO" = "archlinux" ]; then
    sudo pacman -S git cmake 

    BOOST_LIBS="/usr/lib"
    BOOST_INCLUDE="/usr/include"
    
  fi


  # CLONING REPOSITORIES ------------------------------------------------------------------------
  (

  if [ ! -d "repositories" ]; then
  	mkdir repositories
  fi

  cd repositories

  if [ ! -d "supercollider" ]; then
    echo "cloning supercollider repository..."
    git clone --recursive https://github.com/supercollider/supercollider

  elif [ "$1" != "offline" ]; then

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

  elif [ "$1" != "offline" ]; then
    (
      echo "libossia already downloaded..."
      echo "checking for updates..."
      cd libossia
      git pull
      git submodule update --init --recursive
    )
  fi
  )

  # LIBOSSIA BUILD ------------------------------------------------------------------------------

  mkdir -p build
  mkdir -p build/libossia
  mkdir -p install
  (
    cd build/libossia

    # CMake and build libossia
    echo "now building libossia..."
    
    if [ "$DISTRO" = "darwin" ]; then

        cmake ../../repositories/libossia -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../../install/libossia -DOSSIA_PYTHON=0 -DOSSIA_NO_QT=1 -DOSSIA_TESTING=0 -DOSSIA_STATIC=1 -DOSSIA_NO_SONAME=1 -DOSSIA_PD=0 -DBOOST_ROOT=$BOOST_ROOT

   elif [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "elementary" ]; then
    ../../dependencies/cmake-3.9.3-Linux-x86_64/bin/cmake  ../../repositories/libossia -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../../install/libossia -DOSSIA_PYTHON=0 -DOSSIA_NO_QT=1 -DOSSIA_TESTING=0 -DOSSIA_STATIC=1 -DOSSIA_NO_SONAME=1 -DOSSIA_PD=0 -DBOOST_INCLUDEDIR=$BOOST_INCLUDE -DBOOST_LIBRARYDIR=$BOOST_LIBS -DCMAKE_C_COMPILER=/usr/bin/gcc-7 -DCMAKE_CXX_COMPILER=/usr/bin/g++-7
    
   fi
   
   make -j8
   echo "libossia built succesfully... installing"
   make install

    # Cleaning up
    rm -rf libossia build

  )

  # COPY & OVERWRITES ------------------------------------------------------------------------
  (

  cd repositories/supercollider

  if [ "$1" != "offline" ]; then
      git checkout 3.9
  fi

  cd SCClassLibrary
  if [[ ! -d "Ossia" ]]; then
      mkdir Ossia
  fi
  )

  (
    
  yes | cp -rf Ossia/Overwrites/root/CMakeLists.txt repositories/supercollider
  yes | cp -rf Ossia/Overwrites/lang/CMakeLists.txt repositories/supercollider/lang
  yes | cp -rf Ossia/Overwrites/lang/PyrPrimitive.cpp repositories/supercollider/lang/LangPrimSource
  yes | cp -rf Ossia/Overwrites/testsuite/CMakeLists.txt repositories/supercollider/testsuite/supernova
  yes | cp -rf Ossia/Classes/ossia.sc repositories/supercollider/SCClassLibrary/Ossia
  yes | cp -rf Ossia_light repositories/supercollider/SCClassLibrary
  yes | cp -rf Ossia/HelpSource/Guides/OssiaReference.schelp repositories/supercollider/HelpSource/Guides
  yes | cp -rf Ossia/HelpSource/Classes repositories/supercollider/HelpSource/Classes
  yes | cp -rf Ossia/HelpSource/Help.schelp repositories/supercollider/HelpSource

  shopt -s dotglob nullglob
  mv repositories/supercollider/HelpSource/Classes/Classes repositories/supercollider/HelpSource/Classes/Ossia
  mv repositories/supercollider/HelpSource/Classes/Ossia/* repositories/supercollider/HelpSource/Classes/
  rm -rf repositories/supercollider/HelpSource/Classes/Ossia

  cd install/libossia/include
  if [[ ! -d "ossia-sc" ]]; then
      mkdir ossia-sc
  fi
  )

  # move the ossia prim header into ossia include directory... 
  # the header should maybe be present in libossia repository...
  yes | cp -rf Ossia/Primitives/pyrossiaprim.h install/libossia/include/ossia-sc

  # SUPERCOLLIDER BUILD ----------------------------------------------------------------------
  (
  cd repositories/supercollider

  # remove packaged boost, which gets somehow included even with SYSTEM_BOOST=ON
  rm -rf external_libraries/boost
  )

  mkdir -p build/supercollider
  cd build/supercollider

  if [ "$DISTRO" = "darwin" ]; then 
      cmake ../../repositories/supercollider -DCMAKE_PREFIX_PATH=$QT_PATH -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../../install/supercollider -DSYSTEM_BOOST=ON -DBoost_DIR=$BOOST_ROOT

  elif [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "elementary" ]; then 
    ../../dependencies/cmake-3.9.3-Linux-x86_64/bin/cmake ../../repositories/supercollider -DCMAKE_C_COMPILER=/usr/bin/gcc-7 -DCMAKE_CXX_COMPILER=/usr/bin/g++-7 -DCMAKE_PREFIX_PATH=$QT_PATH -DCMAKE_BUILD_TYPE=Release -DSYSTEM_BOOST=ON -DBOOST_INCLUDEDIR=$BOOST_INCLUDE -DBOOST_LIBRARYDIR=$BOOST_LIBS -DBOOST_NO_SYSTEM_PATHS=ON -DBOOST_ROOT=$BOOST_ROOT
  fi

  make -j8
  sudo make install


