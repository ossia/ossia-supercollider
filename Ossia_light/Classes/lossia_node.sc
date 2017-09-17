LOSSIA_Node {

	classvar instanceCount;

	var <>name, <>parentContainer, <>node, <>oscPort;
	var <>address;

	var <parameter_array;
	var <container_array;
	var <oscPort;

	*new { |address, parentContainer, node, oscPort|
		^this.newCopyArgs(address, parentContainer, node, oscPort).init
	}

	init {

		// global
		instanceCount !? { instanceCount = instanceCount + 1 };
		instanceCount ?? { instanceCount = 1 };

		if(name.beginsWith("/"))
		{ name = name.drop(1) }; // name != address
		address = "/" ++ name;

		// init arrays
		parameter_array = [];
		container_array = [];

		// register to parent container
		parentContainer !? { parentContainer.registerContainer(this) };

	}

	getDirectHierarchy {

		var branch_number, children = [];

		branch_number = address.split($/).size;

		container_array.do({|container_target|
			if(container_target.address.split($/).size == (branch_number + 1))
			{ children = children.add(container_target.name) };
		});

		parameter_array.do({|parameter_target|
			if(parameter_target.address.split($/).size == (branch_number + 1))
			{ children = children.add(parameter_target.name) };
		});

		^children;

	}

	getDirectParameters {

		var branch_number, children = [];
		branch_number = address.split($/).size;

		parameter_array.do({|parameter_target|
			if(parameter_target.address.split($/).size == (branch_number + 1))
			{ children = children.add(parameter_target) };
		});

		^children

	}

	registerParameter { |parameter_target| // accessed by parameters

		parameter_target.defName = this.name ++ "_" ++ parameter_target.name;
		parameter_target.address = this.address ++ "/" ++ parameter_target.name;
		parameter_target.oscPort = this.oscPort;
		parameter_target.defaultNode = this.node;

		this.addParameter(parameter_target);
		parentContainer !? { parentContainer.addParameter(parameter_target) };
	}

	addParameter { |parameter_target| // must be left independent
		parameter_array = this.parameter_array.add(parameter_target);
	}

	registerContainer { |container_target| // accessed by children containers

		// setting
		container_target.address = this.address ++ container_target.address;
		container_target.oscPort = this.oscPort;
		container_target.node = this.node;
		container_target.parameter_array.do({|parameter_target|
			parameter_target.address = this.address ++ parameter_target.address;
		});

		container_target.container_array.do({|target|
			target.address = this.address ++ target.address;
		});

		// merging accesses & addresses
		this.addContainer(container_target);
		parentContainer !? { parentContainer.addContainer(container_target) };
	}

	addContainer { |container_target|
		container_array = this.container_array.add(container_target);
		container_array = this.container_array ++ container_target.container_array;
		parameter_array = this.parameter_array ++ container_target.parameter_array;
	}

	makeSynthArray { |iter| var synth_array = [];

		parameter_array.do({|parameter|
			synth_array = synth_array.add(parameter.defName);

			// Warning: if value is array, report to |iter| argument within Synth send iteration

			if(parameter.val.isArray) {
				synth_array = synth_array.add(parameter.val[iter])} { // else
					synth_array = synth_array.add(parameter.val)};
		});

		^synth_array

	}

}

