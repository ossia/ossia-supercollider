# ossia-supercollider : libossia bindings for supercollider

## Installing

### macOS

- recursive-clone this repository (`git clone --recursive https://github.com/OSSIA/ossia-supercollider.git`), cd into it
- make sure homebrew is installed, otherwise run `/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`
- run ./build.sh
- resulting .app will be located in `install/supercollider/SuperCollider` 

the script clones a fresh version of supercollider (as of this day, version 3.9dev - you may change to sc3.8 branch on macOS, but it doesn't work on Linux), adds and builds the libossia dependencies for sclang. Depending on your connection, the install process may take a little while, because of the complete download and build of the two libs/applications.

### Linux (tested on debian-like systems, unstable on archlinux for now)

- recursive-clone this repository (`git clone --recursive https://github.com/OSSIA/ossia-supercollider.git`), cd into it

if you have boost 1.65 already installed on your system:
- run `./build.sh online /path/to/boost/include/dir /path/to/boost/libs/dir` 
- e.g `./build.sh online /usr/local/include /usr/local/lib`

otherwise:
- just run `./build.sh` this will install the required boost 1.65 libraries manually and locally (this may take quite a while)
- local install folder with sc binaries will be located in `install/supercollider/bin` (you might want to add them in your $PATH)

builds on linux are not as thoroughly tested as on macOS, please report any problem with the building process in the Issues section.

### Documentation
the install provides a guide in the SuperCollider HelpSource's home (the one displayed at startup in the sc-ide)
it is also available here: https://ossia.github.io/?javascript#introduction

#### quick usage example

```js
(
d = OSSIA_Device("ossia-collider");
d.exposeOSCQueryServer(1234, 5678, {
  ~freq = OSSIA_Parameter(d, 'frequency', Float, [0, 20000], 440);
  ~mul = OSSIA_Parameter(d, 'mul', Float, [0, 1], 0.125);
  ~pan = OSSIA_Parameter(d, 'pan', Float, [-1, 1], 0);
});
)

// see the tree-structure of your device
// see OSSIA.score if you want to start making a scenario out of it
"http://localhost:5678".openOS();
"https://github.com/OSSIA/score".openOS();

s.boot();

SynthDef('sinosc', {
	Out.ar(0, Pan2.ar(SinOsc.ar(~freq.kr, 0, ~mul.kr), ~pan.kr));
}).add;

// create synth with parameters' current values
x = Synth('sinosc', d.snapshot);

// now every change in the parameters' values will be reported on the sc-server
~freq.value = 220;
~pan.value = 1;
~mul.value = 0.125;
```


