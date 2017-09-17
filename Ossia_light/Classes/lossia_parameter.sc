LOSSIA_Parameter {

	classvar instanceCount;

	var <container, <name, <type, <range, <>default, <>alwaysOnServ;
	var <>inUnit, <>outUnit, <>sr;
	var <>defaultNode;
	var val, <absolute_val;
	var <address, <>defName;
	var oscFunc, <>oscPort;
	var <>parentAccess;
	var listening, netaddr_responder, responder_device;
	var <>bound_to_ui, <>ui, ui_type;
	var <>description;
	var <kbus, <abus;
	var midifunc, midi_cc_num;


	*new { |container, name, type, range, default, alwaysOnServ = true,
		inUnit, outUnit, sr = 44100|
		^this.newCopyArgs(container, name, type, range, default, alwaysOnServ,
			inUnit, outUnit, sr).init
	}

	init {

		instanceCount !? { instanceCount = instanceCount + 1 };
		instanceCount ?? { instanceCount = 1 };

		abus = inf;
		kbus = inf;

		bound_to_ui = false;
		listening = false;
		description = "no description available";

		// register to parent container
		container.registerParameter(this);
		this.val_(default, onServ: false);
		this.initOSC();

	}

	// OSC & MIDI

	address_{ |newAddress|
		address = newAddress;
		this.initOSC();
	}

	initOSC {
		oscFunc !? { oscFunc.free() };
		oscFunc = OSCFunc({|msg, time, addr, recvPort|
			msg.postln;
			this.val_(msg[1])}, address, nil, oscPort);
	}

	midiLearn { // tbi
		midifunc = MIDIFunc.cc({|v, num|
			this.midiLearnResponder(num);
		});
	}

	midiLearnResponder { |ccnum| // TBI
		("MIDI Learn, cc number" + ccnum).postln;
		midifunc = nil;
		midifunc = MIDIFunc.cc({|v, num|
			var tmp_range = (range[1] - range[0]), res;
			if(v < 100) {
				switch(type)
				{Integer} {}
				{Float} {};
			} {
				v = (128-v);
				switch(type)
				{Integer} {}
				{Float} {};
			};

		}, ccnum);

	}

	// PUSH LEARNING

	pushLearn { |n|
		n !? { this.pushLearnResponder(n) };
		n ?? { midifunc = MIDIFunc.cc({|v, num|
			this.pushLearnResponder(num);
		})};
	}

	pushLearnResponder { |ccnum| // seems ok for now

		("PUSH Learn cc number" + ccnum).postln;

		midifunc !? { midifunc.free() };
		midifunc = MIDIFunc.cc({|v, num|
			var phase = (range[1] - range[0]) / 100, res;

			if(v < 100) {
				switch(type)
				{Integer} { res = this.val(true) + 1}
				{Float} { res = this.val(true) + (phase*v)};
			} {
				v = (128 - v);
				switch(type)
				{Integer} { res = this.val(true) - 1}
				{Float} { res = this.val(true) - (phase*v) }
			};

			this.val_(res);


		}, ccnum);

	}

	pushUnlearn {
		midifunc.free;
		midifunc = nil;
	}

	oscLearn { // tbi

	}

	// MODULATION

	enableModulation { |server, type = \control|
		switch(type)
		{\control} { kbus = Bus.control(server, 1)}
		{\audio} { abus = Bus.audio(server, 1)};
	}

	disableModulation {
		kbus !? {
			kbus.free;
			kbus = nil;
		};
		abus !? { abus.free; abus = nil };
	}

	// CORE

	unitCheck { |value|

		case

		{ (inUnit == \dB) && (outUnit == \amp) } { val = value.dbamp }
		{ (inUnit == \amp) && (outUnit == \dB) } { val = value.ampdb }
		{ (inUnit == \s) && (outUnit == \ms) } { val = value * 1000 }
		{ (inUnit == \s) && (outUnit == \samps) } { val = value * sr }
		{ (inUnit == \ms) && (outUnit == \s) } { val = value/1000 }
		{ (inUnit == \ms) && (outUnit == \freq) } { val = (value/1000).reciprocal }
		{ (inUnit == \ms) && (outUnit == \samps) } { val = (value/1000) * sr }
		{ (inUnit == \samps) && (outUnit == \s) } { val = value/sr }
		{ (inUnit == \samps) && (outUnit == \ms) } { val = (value/sr) * 1000 }


		{ (inUnit == \semitones) && (outUnit == \ratio)}
		{ val = MGU_conversionLib.st_ratio(value)}
		{ (inUnit == \ratio) && (outUnit == \semitones)}
		{ val = MGU_conversionLib.ratio_st(value)}
		{ (inUnit == \midi) && (outUnit == \freq) } { val = value.midicps }
		{ (inUnit == \freq) && (outUnit == \midi) } { val = value.cpsmidi }

	}

	val { |beforeConversion = false|
		var res;
		if(beforeConversion) { res = absolute_val } { res = val };
		^res
	}

	val_ { |value, node, interp = false, duration = 2000, curve = \lin, onServ,
		absolute_unit = false, report_to_ui = true, callback = true|

		var process;

		process = {
			value.size.do({|i|
				// casts & type tests
				if((value[i] == inf) || (value[i] == -inf), {
					value.put(i, value[i].asInteger)});

				if((value[i].isKindOf(Integer)) && (type == Float), {
					value[i] = value[i].asFloat});

				if((value[i].isKindOf(Float)) && (type == Integer), {
					value[i] = value[i].asInteger});

				if((value[i].isKindOf(type) == false) && (type != Array), {
					Error("[PARAMETER] /!\ WRONG TYPE:" + name).throw });

				// range check
				if((value[i].isKindOf(Integer)) || (value[i].isKindOf(Float)), {
					value[i] = value[i].clip(range[0], range[1]);
				});

				if(type == Array){ val = value } { val = value[i] };

				absolute_val = val;

				// unit conversion
				if((inUnit.notNil) && (outUnit.notNil) && (absolute_unit == false)) {
					this.unitCheck(val)
				};

				// sending value on server
				if(onServ) {
					node[0] ?? { node = [defaultNode] };
					node[0] ?? { Error("[PARAMETER] /!\ NODE NOT DEFINED" + name).throw };
					node[i].set(defName, val);
				};
			});

			// call parent methods
			if(parentAccess.notNil && callback) { parentAccess.parameterCallBack(name, value)};

			// reply to minuit listening
			if(listening)
				{ netaddr_responder.sendBundle(nil, [responder_device ++ ":listen",
					address ++ ":value", absolute_val])};

			// reply to gui element
			if((bound_to_ui) && (report_to_ui)) { ui.value_from_parameter(absolute_val) };

		};

		// STARTS HERE
		onServ ?? { if(alwaysOnServ) { onServ = true } { onServ = false }};

		// node & value always as array
		if(node.isArray == false) { node = [node] };

		// if value is a function -> doesn't seem to work atm ?
		if(value.isFunction, {
			var func, currentVal = [];
			func = value;
			if(onServ, {
				node.size.do({|i|
					node[i].get(defName.asSymbol, {|kval|
						currentVal = currentVal.add(kval);
						value = func.value(currentVal);
						process.value})}, { // else -> not on server
						value = func.value(val);
						process.value});
			});
			}, { if(value.isArray == false, { value = [value] });  // else : not a function
			 	 process.value});

	}

	// MINUIT
	enableListening { |netaddr, device| // accessed by minuit interfaces only
		listening = true;
		netaddr_responder ?? { netaddr_responder = netaddr };
		responder_device ?? { responder_device = device };
	}

	disableListening { listening = false }

	// CONVENIENCE DEF MTHODS (change to kr, ar etc.)
	smb {^defName.asSymbol}
	kr {^defName.asSymbol.kr}
	ar {^defName.asSymbol.ar}
	tr {^defName.asSymbol.tr}
	defNameKr {^("\\" ++ defName ++ ".kr")}
	defNameAr {^("\\" ++ defName ++ ".ar")}
	defNameTr {^("\\" ++ defName ++ ".tr")}

}