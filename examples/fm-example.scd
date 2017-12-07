(
// --------------------------------------------------------------------------------------------------------
s.options.sampleRate = 44100;
s.boot;
n = 4;
// --------------------------------------------------------------------------------------------------------
s.waitForBoot({
// --------------------------------------------------------------------------------------------------------
	~device = OSSIA_Device("fmcollider").exposeOSCQueryServer(1234, 5678, {
// --------------------------------------------------------------------------------------------------------
		// define array of parameters in device tree-structure
		~carfreq = OSSIA_Parameter.array(n, ~device, "/synth.%/carfreq", Float, [0, 20000], 220, 'clip');
		~modratio = OSSIA_Parameter.array(n, ~device, "/synth.%/modulation/ratio",
			Float, [0.01, 10], 2, 'clip');
		~modindex = OSSIA_Parameter.array(n, ~device, "/synth.%/modulation/index", Float,
			[0.01, 1000], 100, 'clip');
		~gain = OSSIA_Parameter.array(n, ~device, "/synth.%/gain", Float, [-100, 12], 0, 'clip');
		~pan = OSSIA_Parameter.array(n, ~device, "/synth.%/pan", Float, [-1, 1], 0, 'clip');
		~cutoff = OSSIA_Parameter.array(n, ~device, "/synth.%/cutoff/frequency",
			Float, [20, 20000], 20000, 'clip');

		~master = OSSIA_Parameter(~device, '/master/gain', Float, [-100, 12], 0.2);
// --------------------------------------------------------------------------------------------------------
		// start/stop parameters
		~start = OSSIA_Parameter(~device, 'start', Impulse)
		// make a snapshot of ~device excluding ~start and ~stop parameters
		.callback = { Synth('fmdef', ~device.snapshot(~start, ~stop) };

		~stop = OSSIA_Parameter(~device, 'stop', Impulse)
		.callback = { s.freeAll };
// --------------------------------------------------------------------------------------------------------
		~def = SynthDef('fmdef', {
			var fm = Mix.new(
				Array.fill(n, { |i|
					Pan2.ar(
						LPF.ar(
							SinOsc.ar(~carfreq[i].kr +
								SinOsc.ar(~carfreq[i].kr * ~modratio[i].kr, 0, ~modindex[i].kr),
								0, ~gain[i].kr.dbamp),
							~cutoff[i].kr),
						~pan[i].kr);
			}));
				Out.ar(0, fm * ~master.kr.dbamp);
		}).add;
	});
});
// --------------------------------------------------------------------------------------------------------
)