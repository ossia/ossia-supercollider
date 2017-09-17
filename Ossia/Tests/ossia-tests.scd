
(
d = OSSIA_Device.newOSCQueryServer('supersoftware', 1234, 5678, {
	n = OSSIA_Node(d, 'basic_types');
	f = OSSIA_Parameter(n, 'float', Float, [0, 1], 0.75);
	i = OSSIA_Parameter(n, 'int', Integer, [0, 127, [64, 78, 120]], 64);
	b = OSSIA_Parameter(n, 'bool', Boolean, nil, true);
	c = OSSIA_Parameter(n, 'char', Char, nil, $c); // no
	t = OSSIA_Parameter(n, 'string', String, nil, "foo");
	p = OSSIA_Parameter(n, 'pulse', Impulse);
	v = OSSIA_Node(d, 'array_types');
	w = OSSIA_Parameter(v, 'vec2f', OSSIA_vec2f, [[0.5, 0.5], [1.0, 1.0]], OSSIA.vec2f(0.5, 0.75));
	x = OSSIA_Parameter(v, 'vec3f', OSSIA_vec3f, [0.5, 1.0], [0.5, 0.5, 0.5]);
	y = OSSIA_Parameter(v, 'vec4f', OSSIA_vec4f, [0.3, 5.0], [0.25, 0.5, 0.75, 0]);
	t = OSSIA_Parameter(v, 'list', Array, nil, ['foo', "bar", 1.25, [435, true]]); // no
});
)

i.domain;

w.domain;

d.free();

f.critical = true;

d.delete();

d.save_preset(); // ok
v.save_preset(); // ok

d.load_preset();

t.value = ["foo", 3243, 43.5, true];
t.value = ["foo", 'bar', 435, true, [24.5, false]];
t.value = [324, true, ["hello", 0.1]];
t.value;
t.critical = false;
t.critical = true;
t.fullpath;

f.critical = true;
f.value = 3.32;

t.callback = {|v| v.postln };

z = NetAddr("127.0.0.1", 1234);
z.sendMsg(t.fullpath, 234532, "hello", ["plouf", 232]);

c.value = 'z';
c.value = $e;

c.value;

c.value = $r;

t.critical = true;

// LEFT2DO:

// return values for tuple : ok, but bug for sending to i-score with OSC & WS
// domain...
// critical fails
// return vecf values as true vecf classes...

// check_argument_type should return std::string
// zombie
// tags
// optional stuff
// tests
// helpfiles!!!

// oscquery mirror stuff







