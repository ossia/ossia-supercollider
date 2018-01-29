(
d = OSSIA.device("ossia-collider").exposeOSCQueryServer(1234, 5678, {
  ~freq = OSSIA.parameter(d, 'frequency', Float, [0, 20000], 440);
  ~mul = OSSIA.parameter(d, 'mul', Float, [0, 1], 0.125);
  ~pan = OSSIA.parameter(d, 'pan', Float, [-1, 1], 0);
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

