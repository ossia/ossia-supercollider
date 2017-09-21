
// MIRROR TEST
(

~device = OSSIA_Device('my_device').exposeOSCQueryServer(1234, 5678, {

	~basic_types = OSSIA_Node(d, 'basic_types');
	~float = OSSIA_Parameter(n, 'my_float', Float, [-5, 5], 3);
	~integer = OSSIA_Parameter(n, 'my_int', Integer, [0, 127, [0, 32, 64, 96]], 32, 'clip');
	~string = OSSIA_Parameter(n, 'my_string', String, [nil, nil, ['play', 'stop']], 'stop', 'clip');
});

)

m = OSSIA_Device('mirror').exposeOSCQueryMirror("ws://localhost:5678");
m.children;

i = OSSIA_MirrorParameter(m, '/my_float');
i.value;
i.value = 7;

p.value;

//tcp : 8898
//udp : 7741


d = OSSIA_Device("hello").exposeOSCQueryServer(7741, 8898);

u = OSSIA_Parameter(d, 'string', String);
u.value = "lol";
u.value;

l = OSSIA_Parameter(d, "list", List);
l.value = [0, 32.5, 'tata'];
l.value;

p = OSSIA_Parameter(d, 'test', Float, critical: true);
p.callback = { |v| v.postln; };


b = OSSIA_Parameter(d, 'tutu', Boolean, default_value: false);


v = OSSIA_Parameter(d, 'tata', OSSIA_vec2f);

v.value = [0.5, 2.0];
v.value;

v.callback = { |value| value.postln };



