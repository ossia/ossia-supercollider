LOSSIA_MinuitDevice {

	var <address, <port, <>print;
	var <parameter_array;
	var <container_array;
	var <respAddr;

	*new { |address = "superColl", port = 3127, print = false|
		^this.newCopyArgs(address, port, print).init
	}

	init {

		respAddr = NetAddr("127.0.0.1", 13579);

		// init arrays
		parameter_array = [];
		container_array = [];

		// init OSC responders
		OSCFunc({|msg, time, addr, recvPort|
			if(print) { msg.postln };
			this.parseDiscovery(msg[1])}, "i-score?namespace", nil, port);

		OSCFunc({|msg, time, addr, recvPort|
			if(print) { msg.postln };
			this.parseGet(msg[1])}, "i-score?get", nil, port);

		OSCFunc({|msg, time, addr, recvPort|
			if(print) { msg.postln };
			this.parseListen(msg[1], msg[2])}, "i-score?listen", nil, port);

	}

	respAddr_ { |ip, port|
		respAddr = NetAddr(ip, port);
	}

	addParameter { |parameter_target|
		this.parameter_array = this.parameter_array.add(parameter_target);
	}

	addContainer { |container_target|
		container_target.oscPort = this.port;
		container_array = this.container_array.add(container_target);
		container_array = this.container_array ++ container_target.container_array;
		parameter_array = this.parameter_array ++ container_target.parameter_array;
	}

	// DISCOVERY

	parseDiscovery { |discovery_msg|
		if(discovery_msg == '/')
		{ this.queryTreeRoot } // else
		{ this.queryNode(discovery_msg)};
	}

	queryTreeRoot {
		var application_nodes;
		application_nodes = this.getApplicationNodes;
		this.sendResponse("namespace", '/', \Application, application_nodes, attributes:
			[\debug, \version, \type, \name, \author]);
	}

	getApplicationNodes {
		var nodes = [];
		container_array.do({|container_target|
			if(container_target.address.split($/).size == 2)
			{ nodes = nodes.add(container_target.address.drop(1).asSymbol)};
		});

		^nodes;
	}

	queryNode { |node| var node_access, node_type, sub_nodes = [], attributes = [];

		node = node.asString;

		container_array.do({|container_target| // node is container ?
			if(node == container_target.address)
			{ node_access = container_target};
		});

		node_access ?? { // then node is parameter
			parameter_array.do({|parameter_target|
				if(node == parameter_target.address)
				{ node_access = parameter_target};
			});
		};

		// if still nothing.. err
		node_access ?? { Error("Minuit: error, couldn't find node:" + node).throw };

		if(node_access.isKindOf(LOSSIA_Node)) {
			node_type = \Container;
			sub_nodes = node_access.getDirectHierarchy();
			attributes = [\tag, \service, \description, \priority];
		} { // else
			node_type = \Data;
			attributes = [\type, \rangeBounds, \service,  \value]
		};

		this.sendResponse("namespace", node, node_type, sub_nodes, attributes: attributes);
	}

	// GET

	parseGet { |get_msg| var node, attribute;

		get_msg = get_msg.asString.split($:);
		node = get_msg[0];
		attribute = get_msg[1];
		this.getQuery(node, attribute);

	}

	getQuery { |node, attribute| var node_access, node_type, response, attribute_value;

		container_array.do({|container_target|
			if(node == container_target.address)
			{ node_access = container_target }});

		node_access ?? {
			parameter_array.do({|parameter_target|
				if(node == parameter_target.address)
				{ node_access = parameter_target };
			});
		};

		node_access ?? { Error("Minuit: error, couldn't find node:" + node).throw };

		switch(attribute,
			nil, { attribute_value = [node_access.val(true)]},
			"rangeBounds", { attribute_value = node_access.range },
			"service", { if(node_access.isKindOf(LOSSIA_Node))
				{ attribute_value = ["model"]}
				{ attribute_value = ["parameter"] }},
			"value", { attribute_value = [node_access.val(true)]},
			"priority", { attribute_value = [0] },
			"type", { switch(node_access.type,
				Float, { attribute_value = [\decimal] },
				Integer, { attribute_value = [\integer]},
				String, { attribute_value = [\string]},
				Symbol, { attribute_value = [\string]})},
			"rangeClipmode", { attribute_value = [\both] }
		);

		attribute !? { response = node ++ ":" ++ attribute };
		attribute ?? { response = node };

		this.sendResponse("get", response, values: attribute_value);
	}

	// LISTEN

	parseListen { |listen_msg1, listen_msg2| var node, state;
		listen_msg1 = listen_msg1.asString.split($:);
		node = listen_msg1[0];
		state = listen_msg2.asString;
		this.listenQuery(node, state);
	}

	listenQuery { |node, state| var node_access, response;

		response = node ++ ":value";

		parameter_array.do({|parameter_target|
			if(node == parameter_target.address)
			{ node_access = parameter_target };
		});

		// if state = enable, access node to send value whenever it changes
		if(state == "enable") {
			node_access.enableListening(respAddr, address) } {
			node_access.disableListening};

		//this.sendResponse("listen", response, values: [node_access.val(true)]);
	}

	sendResponse { |msg_type, subject_node, node_type, nodes, values, attributes|

		var msg_array = [];

		msg_array = msg_array.add(address.asString ++ ":" ++ msg_type.asString);
		msg_array = msg_array.add(subject_node.asSymbol);

		if(msg_type == "namespace") {
			msg_array = msg_array.add(node_type.asSymbol)};

		if((node_type == \Container) || (node_type == \Application)) {
			msg_array = msg_array.add('nodes={');
			msg_array = msg_array ++ nodes;
			msg_array = msg_array.add('}')};

		if(msg_type == "namespace") {
			msg_array = msg_array.add('attributes={');
			msg_array = msg_array ++ attributes;
			msg_array = msg_array.add('}')};

		if(msg_type == "get") {
			values.do({|v|
				msg_array = msg_array.add(v)})};

		if(msg_type == "listen") {
			values.do({|v|
				msg_array = msg_array.add(v)})};

		if(print) { msg_array.postln };

		respAddr.sendBundle(nil, msg_array);
	}

}