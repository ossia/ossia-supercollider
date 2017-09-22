#include "PyrSymbolTable.h"
#include "PyrSched.h"
#include "PyrPrimitive.h"
#include "PyrKernel.h"
#include "PyrSymbol.h"
#include "PyrInterpreter.h"
#include "GC.h"
#include "SC_LanguageClient.h"

#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/oscquery/oscquery_server.hpp>
#include <ossia/network/oscquery/oscquery_mirror.hpp>
#include <ossia-sc/pyrossiaprim.h>
#include <ossia/preset/preset.hpp>
#include <ossia/preset/exception.hpp>
#include <spdlog/spdlog.h>

extern bool compiledOK;

using namespace ossia;
using namespace ossia::sc;
using namespace ossia::net;
using namespace ossia::oscquery;

ex_node_undef   NODE_NOT_FOUND;
ex_arg_val      ARG_BAD_VALUE;
ex_arg_undef    ARG_UNDEFINED;
ex_arg_type     ARG_WRONG_TYPE;

bmap_t g_typemap;
bmap_a g_accessmap;
bmap_b g_bmodemap;

#define SCCBACK_NAME "pvOnCallback"
#define HDR "OSSIA: "
#define WRN_HDR "Warning! "
#define ERR_HDR "Error! "
#define ADRMAXLEN 128

void ERROTP(const std::exception& e, const char* err_type, const char* descr)
{
    std::cout << HDR << err_type << e.what() << descr << std::endl;
}

const char* ossia::sc::ex_node_undef::what() const throw()
{
    return "Couldn't find node. ";
}

const char* ossia::sc::ex_arg_type::what() const throw()
{
    return "Wrong type for argument. ";
}

const char* ossia::sc::ex_arg_undef::what() const throw()
{
    return "Undefined argument. ";
}

const char* ossia::sc::ex_arg_val::what() const throw()
{
    return "Bad value for argument. ";
}

bool ossia::sc::check_argument_definition(pyrslot *s)
{
    if          (IsNil(s))
    throw       ARG_UNDEFINED;
    else        return true;
    return      false;
}

bool ossia::sc::check_argument_type(pyrslot *s, std::initializer_list<std::string> targets)
{
    try     { check_argument_definition(s); }
    catch   ( const std::exception &e) { throw; }

    auto    classname = read_classname(s);
    for     (const auto& target : targets)
    {
        if  (target == classname)
            return true;
    }

    throw       ARG_WRONG_TYPE;
    return      false;
}

template<class T>
int ossia::sc::check_argument_reference
(std::string ref, boost::bimap<std::string, boost::bimaps::multiset_of<T>> target_map) noexcept
{
    typedef     boost::bimap<std::string, boost::bimaps::multiset_of<T>> bm_type;
    typename    bm_type::left_const_iterator l_iter = target_map.left.find(ref);

    if          (l_iter != target_map.left.end())
                return static_cast<int>(l_iter->second);
    return      -1;
}

void ossia::sc::register_sc_node(pyrslot *s, net::node_base *node) noexcept
{
    pyrslot *ptr_var = slotRawObject(s)->slots;
    SetPtr  (ptr_var, node);
}


ossia::net::node_base* ossia::sc::get_node(pyrslot *s)
{
    try { check_argument_type(s, { "OSSIA_Parameter", "OSSIA_Node",
                                   "OSSIA_MirrorParameter", "OSSIA_MirrorNode",
                                   "OSSIA_Device" }); }

    catch( const std::exception &e ) { throw; }
    return (net::node_base*) slotRawPtr(&slotRawObject(s)->slots[0]);
}

template<class T>
T ossia::sc::read_listed_attribute
(pyrslot *s, boost::bimap<std::string, boost::bimaps::multiset_of<T>> target_map)
{
    try                             { check_argument_definition(s); }
    catch(const std::exception &e)  { throw; }

    auto attr        = sc::read_string(s);
    int  ref_check   = check_argument_reference<T>(attr, target_map);

    if ( ref_check >=0 )    return static_cast<T>(ref_check);
    else                    throw  ARG_BAD_VALUE;
}

std::string ossia::sc::read_classname(pyrslot* s) noexcept
{
    PyrClass     *classobj       =   classOfSlot(s);
    std::string  metaclassname   =   slotRawSymbol(&classobj->name)->name;

    if(metaclassname.find("Meta_") == std::string::npos) return metaclassname;
    else return metaclassname.erase(0, 5);
}

std::string ossia::sc::read_string(pyrslot *s)
{
    try     { check_argument_type(s, { "String", "Symbol" }); }
    catch   ( const std::exception& e) { throw; }

    if      ( read_classname(s) == "Symbol" )
            return (std::string) slotSymString(s);
    else
    {
        char v      [ADRMAXLEN];
        slotStrVal  (s, v, ADRMAXLEN);
        return      (std::string) v;
    }
}

char ossia::sc::read_char(pyrslot *s)
{
    try     { check_argument_type(s, { "Char", "Symbol" }); }
    catch   ( const std::exception& e) { throw; }

    if      ( read_classname(s) == "Symbol" )
            return *slotSymString(s);
    else    return s->u.c;
}

float ossia::sc::read_float(pyrslot *s)
{
    try { check_argument_type(s, { "Float", "Integer" }); }
    catch(const std::exception& e) { throw; }

    if(sc::read_classname(s) == "Float")
    {
        float           f;
        slotFloatVal    (s, &f);
        return          f;
    }

    return (float) sc::read_int(s);
}

int ossia::sc::read_int(pyrslot *s)
{
    try     { check_argument_type(s, { "Integer" }); }
    catch   ( const std::exception& e) { throw; }

    int             i;
    slotIntVal      (s, &i);
    return          i;
}

ossia::val_type ossia::sc::read_type(pyrslot *s)
{
    try                             { check_argument_definition(s); }
    catch(const std::exception &e)  { throw; }

    auto mcn         = read_classname(s);
    int  ref_check   = check_argument_reference<ossia::val_type>(mcn, g_typemap);

    if ( ref_check >=0 ) return static_cast<ossia::val_type>(ref_check);
    else                 throw  ARG_WRONG_TYPE;
}

ossia::domain ossia::sc::read_domain(pyrslot *s, ossia::val_type t)
{
    try { check_argument_type(s, { "Array", "List", "OSSIA_domain"}); }
    catch(const std::exception& e) { throw; }

    pyrslot*        target;
    auto            classname   = read_classname(s);
    ossia::domain   domain;

    if  (classname == "List" || classname ==  "Array")
    {
        target      = s;
    }
    else if (classname == "OSSIA_domain")
    {
        target      = slotRawObject(s)->slots;
    }

    auto pre_domain = sc::read_vector<ossia::value>(s, sc::read_value);

    if      (pre_domain.size() == 2)
            domain = ossia::make_domain(pre_domain[0], pre_domain[1]);

    else if (pre_domain.size() == 3)
            domain = ossia::make_domain(pre_domain[0], pre_domain[1],
                     pre_domain[2].get<std::vector<ossia::value>>());

    else throw ARG_BAD_VALUE;

    return domain;
}

template<class T>
std::vector<T> ossia::sc::read_vector(pyrslot *s, T (*getter_function)(pyrslot*))
{
    auto obj = slotRawObject(s);
    std::vector<T> vector;

    for     (int i = 0; i < obj->size; ++i)
            vector.push_back(getter_function(obj->slots+i));

    return  vector;
}

template<class T, class U>
T ossia::sc::read_array(pyrslot *s, U (*getter_function)(pyrslot*))
{
    T       array;

    auto    obj = slotRawObject(s);
    if      (array.size() != obj->size) throw ARG_BAD_VALUE;

    for     (int i = 0; i < obj->size; ++i)
            array[i] = getter_function(obj->slots+i);

    return  array;
}

ossia::value ossia::sc::read_value(pyrslot *s)
{
    auto vtype = sc::read_type(s);
    switch( vtype )
    {
    case val_type::INT:  return ossia::value(read_float(s)); break; // allows int to float conversion in sc...
    case val_type::BOOL: return ossia::value(IsTrue(s)); break;
    case val_type::CHAR: return ossia::value(read_char(s)); break;
    case val_type::FLOAT: return ossia::value(read_float(s)); break;
    case val_type::LIST: return ossia::value(read_vector<ossia::value>(s, read_value)); break;
    case val_type::VEC2F: return ossia::value(read_array<vec2f, float>(s, sc::read_float)); break;
    case val_type::VEC3F: return ossia::value(read_array<vec3f, float>(s, sc::read_float)); break;
    case val_type::VEC4F: return ossia::value(read_array<vec4f, float>(s, sc::read_float)); break;
    case val_type::STRING:  return ossia::value(read_string(s)); break;
    case val_type::IMPULSE: return ossia::impulse{}; break;
    default: throw  ARG_WRONG_TYPE;
    }

    return ossia::value();
}

template<class T>
std::string ossia::sc::format_listed_attribute
(T attribute, boost::bimap<std::string, boost::bimaps::multiset_of<T>> target_map) noexcept
{
    typedef  boost::bimap<std::string, boost::bimaps::multiset_of<T>> bm_type;
    typename bm_type::right_const_iterator r_iter = target_map.right.find(attribute);

    if      (r_iter == target_map.right.end())
            return "nil";
    else    return (r_iter->second);
}

template<class T, class F>
void ossia::sc::write_array(vmglobals *g, pyrslot *target,
const T& values, void (*writer_func)(vmglobals*, pyrslot*, const F&)) noexcept
{
    int sz = values.size();
    auto array = newPyrArray(g->gc, sz, 0, true);
    SetObject(target, array);

    for(int i = 0; i < sz; i++)
    {
        writer_func(g, array->slots+i, values[i]);
        array->size++;
    }
}

void ossia::sc::write_string(vmglobals *g, pyrslot *target, const std::string& string) noexcept
{
    PyrString*  str = newPyrString(g->gc, string.c_str(), 0, true);
    SetObject   (target, str);
}

void ossia::sc::write_value(vmglobals *g, pyrslot *target, const ossia::value& value) noexcept
{    
    ossia::val_type vtype;
    if(value.valid()) vtype = value.getType();
    else vtype = ossia::val_type::NONE;

    switch ( vtype )
    {
    case ossia::val_type::IMPULSE:  SetNil(target); break;
    case ossia::val_type::NONE:     SetNil(target); break;
    case ossia::val_type::BOOL:     SetBool(target, value.get<bool>()); break;
    case ossia::val_type::CHAR:     SetChar(target, value.get<char>()); break;
    case ossia::val_type::FLOAT:    SetFloat(target, value.get<float>()); break;
    case ossia::val_type::INT:      SetInt(target, value.get<int>()); break;
    case ossia::val_type::STRING:   sc::write_string(g, target, value.get<std::string>()); break;
    case ossia::val_type::LIST:     sc::write_array<std::vector<ossia::value>,ossia::value>(g, target, value.get<std::vector<ossia::value>>(), sc::write_value); break;
    case ossia::val_type::VEC2F:    sc::write_array<vec2f,ossia::value>(g, target, value.get<vec2f>(), sc::write_value); break;
    case ossia::val_type::VEC3F:    sc::write_array<vec3f,ossia::value>(g, target, value.get<vec3f>(), sc::write_value); break;
    case ossia::val_type::VEC4F:    sc::write_array<vec4f,ossia::value>(g, target, value.get<vec4f>(), sc::write_value); break;
    }
}

int pyr_instantiate_device(vmglobals *g, int n)
{
    try      { ossia::sc::check_argument_type(g->sp, { "String", "Symbol" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Device name argument.");
        return      errFailed;
    }

    auto device_name        = sc::read_string(g->sp);
    auto mpx_proto_ptr      = std::make_unique<multiplex_protocol>();
    auto device             = new net::generic_device(std::move(mpx_proto_ptr), device_name);

    sc::register_sc_node    (g->sp-1, dynamic_cast<net::node_base*>(device));

    return      errNone;
}

int pyr_expose_oscquery_server(vmglobals *g, int n)
{
    pyrslot  *rcvr           = g->sp-2,
             *pr_osc_port    = g->sp-1,
             *pr_ws_port     = g->sp;

    try      { ossia::sc::check_argument_type(pr_osc_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "OSC Port argument.");
        return      errFailed;
    }

    try      { ossia::sc::check_argument_type(pr_ws_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "WS Port argument.");
        return      errFailed;
    }       

    auto oscq_protocol = std::make_unique<oscquery_server_protocol>
                         (sc::read_int(pr_osc_port), sc::read_int(pr_ws_port));

    auto target_device  = dynamic_cast<net::generic_device*>(sc::get_node(rcvr));
    auto proto_mpx      = dynamic_cast<net::multiplex_protocol*>(&target_device->get_protocol());

    proto_mpx           ->expose_to(std::move(oscq_protocol));

    return               errNone;
}

int pyr_expose_oscquery_mirror(vmglobals *g, int n)
{
    pyrslot     *rcvr       = g->sp-1,
                *pyr_host   = g->sp;

    try      { ossia::sc::check_argument_type(pyr_host, { "String", "Symbol" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Host Address argument.");
        return      errFailed;
    }

    auto mirror_proto_ptr = std::make_unique<oscquery_mirror_protocol>
                           (sc::read_string(pyr_host));

    auto target_device  = dynamic_cast<net::generic_device*>(sc::get_node(rcvr));
    auto multiplex      = dynamic_cast<net::multiplex_protocol*>(&target_device->get_protocol());
    multiplex           ->expose_to(std::move(mirror_proto_ptr));
    mirror_proto_ptr    ->update(*target_device);

    return errNone;
}

int pyr_expose_minuit(vmglobals *g, int n)
{
    pyrslot     *rcvr               = g->sp-3,
                *pyr_remote_ip      = g->sp-2,
                *pyr_remote_port    = g->sp-1,
                *pyr_local_port     = g->sp;

    try      { ossia::sc::check_argument_type(pyr_remote_ip, { "String", "Symbol" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Remote IP argument.");
        return      errFailed;
    }

    try      { ossia::sc::check_argument_type(pyr_remote_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Remote OSCPort argument.");
        return      errFailed;
    }

    try      { ossia::sc::check_argument_type(pyr_local_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Local OSCPort argument.");
        return      errFailed;
    }    

    auto target_device = dynamic_cast<net::generic_device*>(sc::get_node(rcvr));
    auto device_name = target_device->get_name();

    auto minuit_proto = std::make_unique<minuit_protocol>(
                        device_name,
                        sc::read_string(pyr_remote_ip),
                        sc::read_int(pyr_remote_port),
                        sc::read_int(pyr_local_port));

    auto multiplex = dynamic_cast<net::multiplex_protocol*>(&target_device->get_protocol());
    multiplex->expose_to(std::move(minuit_proto));

    return errNone;
}

int pyr_expose_osc(vmglobals *g, int n)
{
    pyrslot     *rcvr               = g->sp-3,
                *pyr_remote_ip      = g->sp-2,
                *pyr_remote_port    = g->sp-1,
                *pyr_local_port     = g->sp;

    try      { ossia::sc::check_argument_type(pyr_remote_ip, { "String", "Symbol" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Remote IP argument.");
        return      errFailed;
    }

    try      { ossia::sc::check_argument_type(pyr_remote_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Remote OSCPort argument.");
        return      errFailed;
    }

    try      { ossia::sc::check_argument_type(pyr_local_port, { "Integer" }); }
    catch    ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Local OSCPort argument.");
        return      errFailed;
    }

    auto target_device = dynamic_cast<net::generic_device*>(sc::get_node(rcvr));
    auto multiplex = dynamic_cast<net::multiplex_protocol*>(&target_device->get_protocol());

    auto osc_proto = std::make_unique<osc_protocol>(
                sc::read_string(pyr_remote_ip),
                sc::read_int(pyr_remote_port),
                sc::read_int(pyr_local_port));

    multiplex->expose_to(std::move(osc_proto));

    return errNone;
}

int pyr_instantiate_node(vmglobals *g, int n)
{        
    pyrslot  *rcvr       = g->sp-2,
             *pr_parent  = g->sp-1,
             *pr_name    = g->sp;

    ossia::net::node_base *parent_node;

    try     { parent_node = ossia::sc::get_node(pr_parent); }
    catch   ( const ossia::sc::ex_arg_type &e)
    {        
        ERROTP      (e, ERR_HDR, "Parent Argument. Aborting...");
        return      errFailed;
    }
    catch   ( const ossia::sc::ex_arg_undef &e)
    {
        ERROTP      (e, ERR_HDR, "Parent Argument. Trying in single-device mode...");
        //! TODO : if a single device, automatically create the node on this device
        return      errFailed;
    }

    auto name = sc::read_string(pr_name);
    if ( parent_node->find_child(name) ) return errFailed;

    auto node = &net::find_or_create_node(*parent_node, name);
    sc::register_sc_node (rcvr, node);

    return errNone;
}

int pyr_instantiate_parameter(vmglobals *g, int n)
{    
    pyrslot
    *pr_repetition_filter   =   g->sp,
    *pr_critical            =   g->sp-1,
    *pr_bounding_mode       =   g->sp-2,
    *pr_default_value       =   g->sp-3,
    *pr_domain              =   g->sp-4,
    *pr_type                =   g->sp-5,
    *pr_name                =   g->sp-6,
    *pr_parent              =   g->sp-7,
    *rcvr                   =   g->sp-8;

    ossia::val_type             type;
    ossia::bounding_mode        bounding_mode;
    ossia::domain               domain;
    ossia::repetition_filter    repetition_filter;
    ossia::value                defvalue;

    // PARENT  ------------------------------------------------
    net::node_base  *parent;
    try             { parent = sc::get_node(pr_parent); }
    catch           ( const std::exception e)
    {
        ERROTP      (e, ERR_HDR, "Parent argument, aborting...");
        return      errFailed;
    }

    // NAME  ------------------------------------------------
    try     { sc::check_argument_type(pr_name, { "String", "Symbol" }); }
    catch   ( const std::exception &e )
    {
        ERROTP      (e, ERR_HDR, "Name argument.");
        return      errFailed;
    }

    auto name = sc::read_string(pr_name);

    // if node already exist, don't increment, return
    if ( parent->find_child(name) ) return errFailed;

    // TYPE  ------------------------------------------------
    try     { type = sc::read_type(pr_type); }
    catch   ( const std::exception &e )
    {        
        try     { type = sc::read_type(pr_default_value); }
        catch   ( const std::exception &exc )
        {
            ERROTP  (e, ERR_HDR, "Type argument. Could not deduce parameter type.");
            return  errFailed;
        }
    }

    if          (type == ossia::val_type::IMPULSE)  goto critical;
    else if     (type == ossia::val_type::BOOL)     goto value;

    // DOMAIN -----------------------------------------------

    try     { domain = sc::read_domain(pr_domain, type); }
    catch   ( const std::exception &e ) {}

    // BOUNDING MODE ---------------------------------------
    bounding_mode = sc::read_listed_attribute<ossia::bounding_mode>
                    (pr_bounding_mode, g_bmodemap);

    value: //------------------------------------------------
    try     { defvalue = sc::read_value(pr_default_value); }
    catch   ( const std::exception &e ) {}

    // REPETITION FILTER -------------------------------------
    repetition_filter   =   static_cast<ossia::repetition_filter>
                            (IsTrue(pr_repetition_filter));

    critical: // ---------------------------------------------
    bool critical       =   IsTrue(pr_critical);

    // SET ---------------------------------------------------
    auto node = &net::find_or_create_node(*parent, name);
    auto parameter = node->create_parameter(type);

    if (type ==  ossia::val_type::IMPULSE ||
        type ==  ossia::val_type::BOOL ) goto common;

    parameter         ->  set_bounding(bounding_mode);
    parameter         ->  set_domain(domain);

    common:
    parameter         ->  set_repetition_filter(repetition_filter);
    parameter         ->  set_critical(critical);
    parameter         ->  push_value(defvalue);

    // UPDATE RECEIVER NODE ID  ---------------------------
    ossia::sc::register_sc_node(rcvr, node);

    // RETURN ---------------------------------------------
    return errNone;
}

int pyr_node_get_mirror(vmglobals *g, int n)
{
    auto node = ossia::net::find_node(*sc::get_node(g->sp-1), sc::read_string(g->sp));
    if  (!node)  throw NODE_NOT_FOUND;
    sc::register_sc_node(g->sp-2, node);
    return errNone;
}

int pyr_node_get_name(vmglobals *g, int n)
{
    sc::write_string(g, g->sp, sc::get_node(g->sp)->get_name());
    return errNone;
}

int pyr_node_get_full_path(vmglobals *g, int n)
{
    auto node           = sc::get_node(g->sp);
    std::string path    = ossia::net::osc_parameter_string_with_device(*node);
    sc::write_string    (g, g->sp, path.c_str());
    return              errNone;
}

int pyr_node_get_children_names(vmglobals *g, int n)
{
    auto node = sc::get_node(g->sp);
    std::vector<std::string> children_names = node->children_names();
    sc::write_array<std::vector<std::string>,std::string>(g, g->sp, children_names, sc::write_string);

    return errNone;
}

int pyr_node_get_description(vmglobals *g, int n)
{
    auto descr = ossia::net::get_description(*sc::get_node(g->sp)).value_or("null");
    sc::write_string(g, g->sp, descr.c_str());
    return errNone;
}

int pyr_node_get_tags(vmglobals *g, int n)
{
    auto tags = ossia::net::get_tags(*sc::get_node(g->sp));
    sc::write_array<std::vector<std::string>,std::string>(g, g->sp, *tags, sc::write_string);
    return errNone;
}

int pyr_parameter_get_value(vmglobals *g, int n)
{
    auto param = sc::get_node(g->sp)->get_parameter();
    sc::write_value(g, g->sp, param->value());
    return errNone;
}

int pyr_parameter_get_access_mode(vmglobals *g, int n)
{
    auto amode          = net::get_access_mode(*sc::get_node(g->sp));
    auto amode_str      = sc::format_listed_attribute<access_mode>(*amode, g_accessmap);
    sc::write_string    (g, g->sp, amode_str);
    return              errNone;
}

int pyr_parameter_get_domain(vmglobals *g, int n)
{
    auto domain = net::get_domain(*sc::get_node(g->sp));
    std::vector<ossia::value> sc_domain;

    auto min = ossia::get_min(domain);
    auto max = ossia::get_max(domain);
    auto values = ossia::get_values(domain);

    if(min.valid() && max.valid())
    {
        sc_domain.push_back(min);
        sc_domain.push_back(max);
    }
    else
    {
        sc_domain.push_back(ossia::value{}); // nil values
        sc_domain.push_back(ossia::value{});
    }

    if(!values.empty()) sc_domain.push_back(values);
    else sc_domain.push_back(ossia::value{});

    sc::write_array<std::vector<ossia::value>,ossia::value>(g, g->sp, sc_domain, sc::write_value);

    return errNone;
}

int pyr_parameter_get_bounding_mode(vmglobals *g, int n)
{
    auto bmode          = *net::get_bounding_mode(*sc::get_node(g->sp));
    auto bmode_str      = sc::format_listed_attribute<bounding_mode>(bmode, g_bmodemap);
    sc::write_string    (g, g->sp, bmode_str);
    return              errNone;
}

int pyr_parameter_get_critical(vmglobals *g, int n)
{
    auto        critical = net::get_critical(*sc::get_node(g->sp));
    SetBool     (g->sp, critical);
    return      errNone;
}

int pyr_parameter_get_repetition_filter(vmglobals *g, int n)
{
    auto        rep_filter = static_cast<bool>
                (net::get_repetition_filter(*sc::get_node(g->sp)));
    SetBool     (g->sp, rep_filter);
    return      errNone;
}

int pyr_parameter_get_unit(vmglobals *g, int n)
{
    auto unit           = net::get_unit(*sc::get_node(g->sp));
    auto unit_txt       = get_pretty_unit_text(unit);
    sc::write_string    (g, g->sp, unit_txt);
    return              errNone;
}

int pyr_parameter_get_priority(vmglobals *g, int n)
{
    auto    priority = *net::get_priority(*sc::get_node(g->sp));
    SetInt  (g->sp, (int) priority);
    return  errNone;
}

int pyr_node_get_disabled(vmglobals *g, int n)
{
    auto    disabled = net::get_disabled(*sc::get_node(g->sp));
    SetBool (g->sp, disabled);
    return  errNone;
}

int pyr_node_get_hidden(vmglobals *g, int n)
{
    auto    hidden = net::get_hidden(*sc::get_node(g->sp));
    SetBool (g->sp, hidden);
    return  errNone;
}

int pyr_node_get_muted(vmglobals *g, int n)
{
    auto muted = net::get_muted(*sc::get_node(g->sp));
    SetBool (g->sp, muted);
    return  errNone;
}

int pyr_node_get_zombie(vmglobals *g, int n)
{
    auto zombie = net::get_zombie(*sc::get_node(g->sp));
    SetBool (g->sp, zombie);
    return errNone;
}

int pyr_parameter_set_value(vmglobals *g, int n)
{
    // problem with this, is, callback is triggered before the end of the primitive
    // makes the interpreter crash.. so we have to set it quiet
    auto value  = sc::read_value(g->sp);
    auto param  = sc::get_node(g->sp-1)->get_parameter();
    param       ->set_value_quiet(value);
    param       ->get_node().get_device().get_protocol().push(*param);
    return      errNone;
}

int pyr_parameter_set_callback(vmglobals *g, int n)
{
    pyrobject* obj  = slotRawObject(g->sp);
    auto param      = sc::get_node(g->sp)->get_parameter();

    param->add_callback([=](const ossia::value& v)
    {
        uint8_t numArgs = 2;
        gLangMutex.lock();

        if (compiledOK)
        {
            g->canCallOS        = true;
            ++g->sp;            SetObject(g->sp, obj);
            ++g->sp;            ossia::sc::write_value(g, g->sp, param->value() );
            runInterpreter      (g, getsym(SCCBACK_NAME), numArgs);
            g->canCallOS        = false;
        }

        gLangMutex.unlock();

    });

    return errNone;
}

int pyr_parameter_remove_callback(vmglobals *g, int n)
{
    auto    param = sc::get_node(g->sp)->get_parameter();
    param   ->callbacks_clear();
    return  errNone;
}

int pyr_parameter_set_access_mode(vmglobals *g, int n)
{
    auto amode  = sc::read_listed_attribute<ossia::access_mode>(g->sp, g_accessmap);
    auto param  = sc::get_node(g->sp-1)->get_parameter();
    param       ->set_access(amode);
    return      errNone;
}

int pyr_parameter_set_domain(vmglobals *g, int n)
{
    auto param  = sc::get_node(g->sp-1)->get_parameter();
    auto domain = sc::read_domain(g->sp, param->get_value_type());
    param       ->set_domain(domain);
    return      errNone;
}

int pyr_parameter_set_bounding_mode(vmglobals *g, int n)
{
   auto bmode   = sc::read_listed_attribute<bounding_mode>(g->sp, g_bmodemap);
   auto param   = sc::get_node(g->sp-1)->get_parameter();
   param        ->set_bounding(bmode);
   return       errNone;
}


int pyr_parameter_set_repetition_filter(vmglobals *g, int n)
{
    auto param      = sc::get_node(g->sp-1)->get_parameter();
    auto rfilter    = static_cast<repetition_filter>(IsTrue(g->sp));
    param           ->set_repetition_filter(rfilter);

    return          errNone;
}

int pyr_parameter_set_unit(vmglobals *g, int n)
{
    auto unit       = ossia::parse_pretty_unit(sc::read_string(g->sp));
    auto param      = sc::get_node(g->sp-1)->get_parameter();
    param           ->set_unit(unit);

    return          errNone;
}

int pyr_parameter_set_priority(vmglobals *g, int n)
{
    ossia::net::set_priority(*sc::get_node(g->sp-1), sc::read_int(g->sp));
    return errNone;
}

int pyr_parameter_set_critical(vmglobals *g, int n)
{
    ossia::net::set_critical(*sc::get_node(g->sp-1), IsTrue(g->sp));
    return errNone;
}

int pyr_node_set_disabled(vmglobals *g, int n)
{
    ossia::net::set_disabled(*sc::get_node(g->sp-1), IsTrue(g->sp));
    return errNone;
}

int pyr_node_set_hidden(vmglobals *g, int n)
{
    ossia::net::set_hidden(*sc::get_node(g->sp-1), IsTrue(g->sp));
    return errNone;
}

int pyr_node_set_muted(vmglobals *g, int n)
{
    ossia::net::set_muted(*sc::get_node(g->sp-1), IsTrue(g->sp));
    return errNone;
}

int pyr_node_set_description(vmglobals *g, int n)
{
    net::set_description(*sc::get_node(g->sp-1), sc::read_string(g->sp).c_str());
    return errNone;
}

int pyr_node_set_tags(vmglobals *g, int n)
{
    auto node = sc::get_node(g->sp-1);
    auto tags = sc::read_vector<std::string>(g->sp, sc::read_string);
    net::set_tags(*node, tags);
    return errNone;
}

int pyr_preset_load(vmglobals *g, int n)
{
    auto node = sc::get_node(g->sp-1);
    std::ifstream ifs(sc::read_string(g->sp));
    std::string json;
    json.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    auto preset = ossia::presets::read_json(json);
    ossia::presets::apply_preset(*node, preset);

    return errNone;
}

int pyr_preset_save(vmglobals *g, int n)
{
    auto node = sc::get_node(g->sp-1);
    auto preset = ossia::presets::make_preset(*node);
    auto json = ossia::presets::write_json(node->get_name(), preset);

    std::ofstream ofs(sc::read_string(g->sp)); ofs << json;

    return errNone;
}

int pyr_free_device(vmglobals *g, int n)
{
    auto    node = sc::get_node(g->sp);
    delete  node;

    auto device_obj = slotRawObject(g->sp);
    g->gc->Free(device_obj);
    SetNil(g->sp);

    return errNone;
}

void initOssiaPrimitives() {

    int base, index = 0;
    base = nextPrimitiveIndex();

    definePrimitive(base, index++, "_OSSIA_InstantiateDevice", pyr_instantiate_device, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ExposeOSCQueryServer", pyr_expose_oscquery_server, 3, 0);
    definePrimitive(base, index++, "_OSSIA_ExposeOSCQueryMirror", pyr_expose_oscquery_mirror, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ExposeMinuit", pyr_expose_minuit, 4, 0);
    definePrimitive(base, index++, "_OSSIA_ExposeOSC", pyr_expose_osc, 4, 0);

    definePrimitive(base, index++, "_OSSIA_InstantiateParameter", pyr_instantiate_parameter, 9, 0);
    definePrimitive(base, index++, "_OSSIA_InstantiateNode", pyr_instantiate_node, 3, 0);

    definePrimitive(base, index++, "_OSSIA_NodeGetName", pyr_node_get_name, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetChildrenNames", pyr_node_get_children_names, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetFullPath", pyr_node_get_full_path, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetDisabled", pyr_node_get_disabled, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetHidden", pyr_node_get_hidden, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetMuted", pyr_node_get_muted, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetDescription", pyr_node_get_description, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetTags", pyr_node_get_tags, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetZombie", pyr_node_get_zombie, 1, 0);
    definePrimitive(base, index++, "_OSSIA_NodeGetMirror", pyr_node_get_mirror, 3, 0);

    definePrimitive(base, index++, "_OSSIA_NodeSetDisabled", pyr_node_set_disabled, 2, 0);
    definePrimitive(base, index++, "_OSSIA_NodeSetHidden", pyr_node_set_hidden, 2, 0);
    definePrimitive(base, index++, "_OSSIA_NodeSetMuted", pyr_node_set_muted, 2, 0);
    definePrimitive(base, index++, "_OSSIA_NodeSetDescription", pyr_node_set_description, 2, 0);
    definePrimitive(base, index++, "_OSSIA_NodeSetTags", pyr_node_set_tags, 2, 0);

    definePrimitive(base, index++, "_OSSIA_ParameterSetValue", pyr_parameter_set_value, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetCallback", pyr_parameter_set_callback, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterRemoveCallback", pyr_parameter_remove_callback, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetAccessMode", pyr_parameter_set_access_mode, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetDomain", pyr_parameter_set_domain, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetBoundingMode", pyr_parameter_set_bounding_mode, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetRepetitionFilter", pyr_parameter_set_repetition_filter, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetUnit", pyr_parameter_set_unit, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetPriority", pyr_parameter_set_priority, 2, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterSetCritical", pyr_parameter_set_critical, 2, 0);

    definePrimitive(base, index++, "_OSSIA_ParameterGetValue", pyr_parameter_get_value, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetAccessMode", pyr_parameter_get_access_mode, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetDomain", pyr_parameter_get_domain, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetBoundingMode", pyr_parameter_get_bounding_mode, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetRepetitionFilter", pyr_parameter_get_repetition_filter, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetUnit", pyr_parameter_get_unit, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetPriority", pyr_parameter_get_priority, 1, 0);
    definePrimitive(base, index++, "_OSSIA_ParameterGetCritical", pyr_parameter_get_critical, 1, 0);

    definePrimitive(base, index++, "_OSSIA_PresetLoad", pyr_preset_load, 2, 0);
    definePrimitive(base, index++, "_OSSIA_PresetSave", pyr_preset_save, 2, 0);

    definePrimitive(base, index++, "_OSSIA_FreeDevice", pyr_free_device, 1, 0);

    g_typemap.insert( bmap_t::value_type("Integer", val_type::INT));
    g_typemap.insert( bmap_t::value_type("Boolean", val_type::BOOL));
    g_typemap.insert( bmap_t::value_type("True", val_type::BOOL));
    g_typemap.insert( bmap_t::value_type("False", val_type::BOOL));
    g_typemap.insert( bmap_t::value_type("Char", val_type::CHAR));
    g_typemap.insert( bmap_t::value_type("Float", val_type::FLOAT));
    g_typemap.insert( bmap_t::value_type("OSSIA_vec2f", val_type::VEC2F));
    g_typemap.insert( bmap_t::value_type("OSSIA_vec3f", val_type::VEC3F));
    g_typemap.insert( bmap_t::value_type("OSSIA_vec4f", val_type::VEC4F));
    g_typemap.insert( bmap_t::value_type("Array", val_type::LIST));
    g_typemap.insert( bmap_t::value_type("List", val_type::LIST));
    g_typemap.insert( bmap_t::value_type("Impulse", val_type::IMPULSE));
    g_typemap.insert( bmap_t::value_type("Signal", val_type::IMPULSE));
    g_typemap.insert( bmap_t::value_type("String", val_type::STRING));
    g_typemap.insert( bmap_t::value_type("Symbol", val_type::STRING));

    g_accessmap.insert( bmap_a::value_type("bi", access_mode::BI));
    g_accessmap.insert( bmap_a::value_type("both", access_mode::BI));
    g_accessmap.insert( bmap_a::value_type("rw", access_mode::BI));
    g_accessmap.insert( bmap_a::value_type("get", access_mode::GET));
    g_accessmap.insert( bmap_a::value_type("read", access_mode::GET));
    g_accessmap.insert( bmap_a::value_type("r", access_mode::GET));
    g_accessmap.insert( bmap_a::value_type("set", access_mode::SET));
    g_accessmap.insert( bmap_a::value_type("write", access_mode::SET));
    g_accessmap.insert( bmap_a::value_type("w", access_mode::SET));

    g_bmodemap.insert( bmap_b::value_type("clip", bounding_mode::CLIP));
    g_bmodemap.insert( bmap_b::value_type("fold", bounding_mode::FOLD));
    g_bmodemap.insert( bmap_b::value_type("free", bounding_mode::FREE));
    g_bmodemap.insert( bmap_b::value_type("high", bounding_mode::HIGH));
    g_bmodemap.insert( bmap_b::value_type("low", bounding_mode::LOW));
    g_bmodemap.insert( bmap_b::value_type("wrap", bounding_mode::WRAP));
}
