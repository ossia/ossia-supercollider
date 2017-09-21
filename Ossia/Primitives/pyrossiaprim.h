#ifndef PYROSSIA_H
#define PYROSSIA_H

#include "VMGlobals.h"
#include <ossia/ossia.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/editor/dataspace/dataspace.hpp>
#include <vector>
#include <iostream>
#include <exception>
#include <initializer_list>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

using pyrslot   = PyrSlot;
using pyrobject = PyrObject;
using vmglobals = VMGlobals;

namespace ossia
{
namespace sc
{
typedef boost::bimap
<std::string, boost::bimaps::multiset_of<ossia::val_type>>  bmap_t;

typedef boost::bimap
<std::string, boost::bimaps::multiset_of<ossia::access_mode>> bmap_a;

typedef boost::bimap
<std::string, boost::bimaps::multiset_of<ossia::bounding_mode>> bmap_b;
/**         boost bimap associating ossia enum types
 *          with valid sc string/symbol values
 */

class ex_node_undef :   public std::exception {
public:                 virtual const char* what() const throw() final; };
/**                     exception: in case ossia node cannot be found on the stack
 *                      useful when trying to get mirror nodes
 */
class ex_arg_val :      public std::exception {
public:                 virtual const char* what() const throw() final; };
/**                     exception: in case value doesn't match requirements
 */
class ex_arg_undef :    public std::exception {
public:                 virtual const char* what() const throw() final; };
/**                     exception: in case slot argument is undefined (nil)
 */
class ex_arg_type :     public std::exception {
public:                 virtual const char* what() const throw() final; };
/**                     exception: in case slot-argument's type doesn't match
 */
bool                    check_argument_type(pyrslot* s, std::initializer_list<std::string> targets);
/**                     returns true if one of the targets matches the slot's classtype,
 *                      returns false and throws error otherwise
 */
bool                    check_argument_definition(pyrslot *s);
/**                     returns true if target is not nil
 *                      returns false and throws error otherwise
 */
template<class T> int   check_argument_reference
                        (std::string ref, boost::bimap<std::string,
                        boost::bimaps::multiset_of<T>> target) noexcept;
/**                     returns target's enum index if argument matches the list
 *                      returns -1 otherwise
 */
void                    register_sc_node(pyrslot *s, net::node_base *node) noexcept;
/**                     saves the node on the stack
 */
int                     get_node_id(pyrslot *s) noexcept;
/**                     gets and returns slot's node_id
 *                      (always first array slot in OSSIA_Node based classes)
 */
net::node_base*         get_node(pyrslot *s);
/**                     returns slot's matching ossia::node
 */
template<class T>
std::string             format_listed_attribute
                        (T attribute, boost::bimap<std::string,
                        boost::bimaps::multiset_of<T>> target_map) noexcept;
/**                     converts ossia type or enum attribute (access & bounding modes) to std::string
 */
template
<class T, class F>
void                    write_array
                        (vmglobals *g, pyrslot *target, const T& values,
                        void (*writer_func)(vmglobals*, pyrslot*, const F&)) noexcept;
/**                     writes array from std::vector to target slot
*/
void                    write_string(vmglobals *g, pyrslot *target, const std::string& string) noexcept;
/**                     writes an std::string to a sc slot
*/
void                    write_value(vmglobals *g, pyrslot *target, const ossia::value& value) noexcept;
/**                     writes an ossia value to a sc slot
 */
std::string             read_classname(pyrslot* s) noexcept;
/**                     returns slot's classname as std::string, safe function
 */
template<class T> T     read_listed_attribute
                        (pyrslot *s, boost::bimap<std::string,
                        boost::bimaps::multiset_of<T>> target_list);
/**                     returns ossia type or attribute (access & bounding modes) from sc String or Symbol slot
 *                      throws exception if target doesn't match
 */
value                   read_value(pyrslot *s);
val_type                read_type(pyrslot *s);
domain                  read_domain(pyrslot *s, val_type t);
unit_t                  read_unit(pyrslot *s);
/**                     sc slot to ossia node attributes
 */
float                   read_float(pyrslot *s);
int                     read_int(pyrslot *s);
std::string             read_string(pyrslot *s);
char                    read_char(pyrslot *s);
template<class T>
std::vector<T>          read_vector(pyrslot *s, T (*getter_function)(pyrslot*));
template
<class T, class U> T    read_array(pyrslot *s, U (*getter_function)(pyrslot*));
/**                     sc slot to cxx/ossia value types
 */
}
}

void                    initOssiaPrimitives();

#endif // PYROSSIA_H
