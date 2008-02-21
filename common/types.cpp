/* PET
 * Platform for Experimentation with efficient HPSG processing Techniques
 * (C) 1999 - 2002 Ulrich Callmeier uc@coli.uni-sb.de
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* operations on types  */

#include "types.h"
#include "bitcode.h"
#include "dag.h"
#include "dumper.h"
#include "hashing.h"

#ifdef FLOP
#include "flop.h"
#else
#include "cheap.h"
#undef SUBTYPECACHE
#endif

using namespace std;

int nleaftypes;
int *leaftypeparent = 0;

static vector<bitcode *> typecode;
static hash_map<bitcode, int> codetable;

static bitcode *temp_bitcode = NULL;
static int codesize;

type_t *apptype = 0;
type_t *maxapp = 0;

// status
int nstatus;
char **statusnames = 0;

// types
type_t ntypes;
type_t first_leaftype;
char **typenames = 0;
char **printnames = 0;
int *typestatus = 0;

type_t last_dynamic;
#ifdef DYNAMIC_SYMBOLS
// dyntypename contains all dynamically needed type names for a parse.
map<string, type_t> dyntypememo ;
vector<string> dyntypename;
vector<int> integer_type_map;
#endif

int BI_TOP, BI_SYMBOL, BI_STRING, BI_CONS, BI_LIST, BI_NIL, BI_DIFF_LIST;

#ifndef FLOP
vector<list<int> > immediateSupertype;
#endif

// attributes
char **attrname = 0;
int nattrs;
int *attrnamelen = 0;
int BIA_FIRST, BIA_REST, BIA_LIST, BIA_LAST, BIA_ARGS;

inline bool is_leaftype(type_t s) {
  assert(is_type(s));
  return s >= first_leaftype;
}

inline bool is_proper_type(type_t s) {
  assert(is_type(s));
  return s < first_leaftype;
}

void initialize_codes(int n) {
  codesize = n;
  temp_bitcode = new bitcode(codesize);
  codetable[bitcode(codesize)] = T_BOTTOM;
  typecode.resize(n);
}

void resize_codes(int n) {
  typecode.resize(n);
}

void register_codetype(const bitcode &b, int i) {
  codetable[b] = i;
}

void register_typecode(int i, bitcode *b) {
  typecode[i] = b;
}

#ifdef HASH_SPACE
namespace HASH_SPACE {
  template<> struct hash<string> {
    inline size_t operator()(const string &key) const {
      int v = 0;
      for(unsigned int i = 0; i < key.length(); i++)
        v += key[i];

      return v;
    }
  };
}

struct string_equal : public binary_function<string, string, bool> {
  inline bool operator()(const string &s1, const string &s2) {
    return s1 == s2;
  }
};

typedef hash_map<string, int, hash< string >, string_equal> string_map;
#else
typedef map<string, int> string_map;
#endif

string_map _typename_memo;

int lookup_type(const char *s) {
  static bool initialized_cache = false;

  if(!initialized_cache) {
    for(int i = 0; i < ntypes; i++)
      _typename_memo[typenames[i]] = i;
    initialized_cache = true;
  }
  
  string_map::iterator pos = _typename_memo.find(s);
  return (pos != _typename_memo.end()) ? (*pos).second : T_BOTTOM;
}

#ifdef DYNAMIC_SYMBOLS
int lookup_symbol(const char *s) {
  int type = lookup_type(s);

  if (type == T_BOTTOM) {
    // is it registered as dynamic type?
    string str = s;
    map<string, int>::iterator pos = dyntypememo.find(str);
    if(pos != dyntypememo.end())
      return (*pos).second;

    // None of the above: register it as new dynamic type
    dyntypememo[str] = last_dynamic ;
    dyntypename.push_back(str);

    last_dynamic++ ;
    return last_dynamic - 1 ;
  }
  else return type;
}

int lookup_unsigned_symbol(unsigned int i) {
  if(integer_type_map.size() <= i) {
    integer_type_map.resize(i + 1, T_BOTTOM);
  }
  if(integer_type_map[i] == T_BOTTOM) {
    char intstring[40];
    sprintf(intstring, "\"%d\"", i);
    integer_type_map[i] = lookup_symbol(intstring);
  }
  return integer_type_map[i];
}

void clear_dynamic_symbols() {
  last_dynamic = ntypes ;
  for(unsigned int i = 0; i < integer_type_map.size(); i++) {
    if(integer_type_map[i] >= last_dynamic) {
      integer_type_map[i] = T_BOTTOM;
    }
  }
  dyntypename.erase(dyntypename.begin(), dyntypename.end()) ;
  dyntypememo.erase(dyntypememo.begin(), dyntypememo.end()) ;
}
#endif

map<string, attr_t> _attrname_memo; 

attr_t lookup_attr(const char *s) {
  map<string, int>::iterator pos = _attrname_memo.find(s);
  if(pos != _attrname_memo.end())
    return (*pos).second;

  for(int i = 0; i < nattrs; i++) {
    if(strcmp(attrname[i], s) == 0) {
      _attrname_memo[s] = i;
      return i;
    }
  }

  _attrname_memo[s] = -1;
  return -1;
}

/** \brief Check, if the given status name \a s is mentioned in the grammar. If
 *  so, return its code.
 */
int lookup_status(const char *s) {
  for(int i = 0; i < nstatus; i++) {
    if(strcmp(statusnames[i], s) == 0) {
      return i;
    }
  }
  return -1;
}

type_t lookup_code(const bitcode &b) {
  hash_map<bitcode, int>::const_iterator pos = codetable.find(b);

  if(pos == codetable.end())
    return -1;
  else 
    return (*pos).second;
}

int get_special_name(settings *sett, char *suff, bool attr = false) {
  char *buff = new char[strlen(suff) + 25];
  sprintf(buff, attr ? "special-name-attr-%s" : "special-name-%s", suff);
  char *v = sett->req_value(buff);
  int id;
#ifdef FLOP
  if(attr) {
    id = attributes.id(v);
    if(id == -1)
      id = attributes.add(v);
  }
  else {
    id = types.id(v);
    if(id == -1) {
      struct type *t = new_type(v, false);
      t->def = sett->lloc();
      t->implicit = true;
      id = types.id(v);
    }
  }
#else 
  id = attr ? lookup_attr(v) : lookup_type(v);
  if(id == -1) {
    string s(buff);
    delete[] buff;
    throw tError(s + ":=" + v + " not defined (but referenced in settings file)");
  }
#endif
  delete[] buff;
  return id;
}

void initialize_specials(settings *sett)
{
  BI_TOP       = get_special_name(sett, "top");
  BI_SYMBOL    = get_special_name(sett, "symbol");
  BI_STRING    = get_special_name(sett, "string");
  BI_CONS      = get_special_name(sett, "cons");
  BI_LIST      = get_special_name(sett, "list");
  BI_NIL       = get_special_name(sett, "nil");
  BI_DIFF_LIST = get_special_name(sett, "difflist");

  BIA_FIRST = get_special_name(sett, "first", true);
  BIA_REST  = get_special_name(sett, "rest", true);
  BIA_LIST  = get_special_name(sett, "list", true);
  BIA_LAST  = get_special_name(sett, "last", true);
  BIA_ARGS  = get_special_name(sett, "args", true);
}

#ifndef FLOP

void undump_symbol_tables(dumper *f)
{
  // nstatus
  nstatus = f->undump_int();

  // npropertypes
  first_leaftype = f->undump_int();

  // nleaftypes
  nleaftypes = f->undump_int();
  ntypes = first_leaftype + nleaftypes;
  last_dynamic = ntypes;

  // nattrs
  nattrs = f->undump_int();

  statusnames = (char **) malloc(sizeof(char *) * nstatus);

  for(int i = 0; i < nstatus; i++)
    statusnames[i] = f->undump_string();

  typenames = (char **) malloc(sizeof(char *) * ntypes);
  typestatus = (int *) malloc(sizeof(int) * ntypes);

  for(int i = 0; i < ntypes; i++)
    {
      typenames[i] = f->undump_string();
      typestatus[i] = f->undump_int();
    }

  attrname = (char **) malloc(sizeof(char *) * nattrs);
  attrnamelen = (int *) malloc(sizeof(int) * nattrs);

  for(int i = 0; i < nattrs; i++)
    {
      attrname[i] = f->undump_string();
      attrnamelen[i] = strlen(attrname[i]);
    }

  initialize_specials(cheap_settings);
}

void undump_printnames(dumper *f)
{
  if(f == 0) // we have no printnames
    {
      printnames = typenames;
      return;
    }

  printnames = (char **) malloc(sizeof(char *) * ntypes);

  for(int i = 0; i < ntypes; i++)
    {
      printnames[i] = f->undump_string();
      if(printnames[i] == 0)
        printnames[i] = typenames[i];
    }
}

void free_type_tables()
{
  if(statusnames != 0)
  {
    free(statusnames);
    statusnames = 0;
  }
  if(typenames != 0)
  {
    free(typenames);
    typenames = 0;
  }
  if(typestatus != 0)
  {
    free(typestatus);
    typestatus = 0;
  }
  if(attrname != 0)
  {
    free(attrname);
    attrname = 0;
  }
  if(attrnamelen != 0)
  {
    free(attrnamelen);
    attrnamelen = 0;
  }
  if(printnames != 0)
  {
    free(printnames);
    printnames = 0;
  }

  delete temp_bitcode;
  delete[] leaftypeparent;
  delete[] apptype;
  delete[] maxapp;
  delete[] featset;
  for(int i = 0; i < nfeatsets; i++)
    delete[] featsetdesc[i].attr;
  delete[] featsetdesc;
}

#endif

#ifdef FLOP

void dump_hierarchy(dumper *f)
{
  int i;

  // bitcodesize in bits
  f->dump_int(codesize);

  // bitcodes for all proper types
  for(i = 0; i < first_leaftype; i++)
    if(leaftypeparent[rleaftype_order[i]] == -1)
      {
        typecode[rleaftype_order[i]]->dump(f);
      }
    else
      LOG(loggerUncategorized, Level::INFO,
          "leaf type conception error a: `%s' (%d -> %d)", 
          type_name(rleaftype_order[i]), i, rleaftype_order[i]);

  // parents for all leaf types
  for(i = first_leaftype; i < ntypes; i++)
    if(leaftypeparent[rleaftype_order[i]] != -1)
      {
        f->dump_int(rleaftype_order[leaftypeparent[leaftype_order[i]]]);
      }
    else
      LOG(loggerUncategorized, Level::INFO,
          "leaf type conception error b: `%s' (%d -> %d)", 
          type_name(rleaftype_order[i]), i, rleaftype_order[i]);
}

#endif

void undump_hierarchy(dumper *f)
{
  codesize = f->undump_int();
  initialize_codes(codesize);
  resize_codes(first_leaftype);
  
  for(int i = 0; i < first_leaftype; i++)
    {
      temp_bitcode->undump(f);
      register_codetype(*temp_bitcode, i);
      register_typecode(i, temp_bitcode);
      temp_bitcode = new bitcode(codesize);
    }

  leaftypeparent = new int[nleaftypes];
  for(int i = 0; i < nleaftypes; i++)
    leaftypeparent[i] = f->undump_int();
}

void
initialize_maxapp()
{
    maxapp = new int[nattrs];
    for(int i = 0; i < nattrs; i++)
    {
        maxapp[i] = 0;
        // the direct access to typedag[] is ok here because no dynamic type
        // can be appropriate for a feature
        dag_node *cval = dag_get_attr_value(typedag[apptype[i]], i);
        if(cval && cval != FAIL)
            maxapp[i] = dag_type(cval);
    }
}

void undump_tables(dumper *f)
{
  // read table mapping types to feature set

  int coding = f->undump_int();

  if(coding == 0)
    dag_nocasts = true;
  else if(coding == 1)
    dag_nocasts = false;
  else
    throw tError("unknown encoding");

  featset = new int[first_leaftype];

  for(int i = 0; i < first_leaftype; i++)
    {
      featset[i] = f->undump_int();
    }

  // read feature sets

  nfeatsets = f->undump_int();
  featsetdesc = new featsetdescriptor[nfeatsets];
  
  for(int i = 0; i < nfeatsets; i++)
    {
      short int na = featsetdesc[i].n = f->undump_short();
      featsetdesc[i].attr = na > 0 ? new short int[na] : 0;

      for(int j = 0; j < na; j++)
        featsetdesc[i].attr[j] = f->undump_short();
    }

  // read appropriate sorts table

  apptype = new int[nattrs];
  for(int i = 0; i < nattrs; i++)
    apptype[i] = f->undump_int();
}

#ifndef FLOP
/** Load the list of immediate supertypes from the grammar file */
void
undumpSupertypes(dumper *f)
{
    for(int i = 0; i < first_leaftype; i++)
    {
        int n = f->undump_short();
        list<int> l;
        for(int j = 0; j < n; j++)
        {
            int t = f->undump_int();
            l.push_back(t);
        }
        immediateSupertype.push_back(l);
    }
}

const list< type_t > &immediate_supertypes(type_t type) {
  assert(is_proper_type(type)) ;
  return immediateSupertype[type];
}

/** Add all supertypes of \a type to \a result, excluding \c *top* but
 *  including \a type itself.
 *  This is an internal helper function for all_supertypes.
 */
void get_all_supertypes(type_t type, hash_set< type_t > &result) {
  // top is a supertype of every type, so we do not add this redundant
  // information 
  if((type == BI_TOP) || (result.find(type) != result.end())) return;
  result.insert(type);
#ifdef DYNAMIC_SYMBOLS
  if (is_dynamic_type(type)) return;
#endif
  if (is_leaftype(type)) {
    get_all_supertypes(leaftype_parent(type), result);
  } else {
    for(list< type_t >::const_iterator it = immediate_supertypes(type).begin();
        it != immediate_supertypes(type).end(); it++) {
      get_all_supertypes(*it, result);
    }
  }
}

typedef hash_map< type_t, list< type_t > > super_map;

super_map all_supertypes_cache;

/** Return the list of all supertypes of \a type */
const list< type_t > &all_supertypes(type_t type) {
  if(all_supertypes_cache.find(type) == all_supertypes_cache.end()) {
    hash_set< type_t > supertypes;
    get_all_supertypes(type, supertypes);
    list<type_t> supers(supertypes.begin(), supertypes.end());
    all_supertypes_cache[type] = supers;
  }
  return all_supertypes_cache[type];
}

#endif

int core_glb(int a, int b)
{
  if(intersect_empty(*typecode[a], *typecode[b], temp_bitcode))
    return T_BOTTOM;
  else
    return lookup_code(*temp_bitcode);
}

bool
core_subtype(type_t a, type_t b)
{
    if(a == b) return true;
    return typecode[a]->subset(*typecode[b]);
}

#ifndef FLOP
#include <typecache.h>
typecache glbcache(0);

#ifdef SUBTYPECACHE
typecache subtypecache(-1);
#endif

void prune_glbcache()
{
  glbcache.prune();
#ifdef SUBTYPECACHE
  subtypecache.prune();
#endif
}

#endif


/** Is \a a a subtype of \a b ? */
bool subtype(int a, int b)
{
  if(a == b) return true; // every type is subtype of itself
  if(b == BI_TOP) return true; // every type is a subtype of top

  if(a == -1) return true; // bottom is subtype of everything
  if(b == -1) return false; // no other type is a subtype of bottom

#ifdef DYNAMIC_SYMBOLS
  if(is_dynamic_type(a)) return false; // dyntypes are only subtypes of BI_TOP
    // return subtype(b, BI_STRING); // dyntypes are direct subtypes of STRING
  if(is_dynamic_type(b)) return false; // and always leaf types
#endif

#ifdef FLOP
  if(leaftypeparent[a] != -1)
    return subtype(leaftypeparent[a], b);
  if(leaftypeparent[b] != -1)
    return false; // only leaftypes can be subtypes of a leaftype
#else
  if(is_leaftype(a))
    return subtype(leaftypeparent[a - first_leaftype], b);
  if(is_leaftype(b))
    return false; // only leaftypes can be subtypes of a leaftype

#ifdef SUBTYPECACHE
  // result is a _reference_ to the cache entry -> automatic writeback
  int &result = subtypecache[ (typecachekey_t) a*ntypes + b ];
  if(result != -1) return result;
  return (result = core_subtype(a, b));
#endif

#endif

  return core_subtype(a, b);
}

#ifndef FLOP
void
subtype_bidir(type_t a, type_t b, bool &forward, bool &backward)
{
    // precondition: a != b, a >= 0, b >= 0
    // postcondition: forward == subtype(a, b) && backward == subtype(b, a)
    
    LOG(loggerUncategorized, Level::DEBUG, "ST %d %d - ", a, b);

    if(a == BI_TOP)
    {
        LOG(loggerUncategorized, Level::DEBUG, "1");
        forward = false;
        backward = true;
        return;
    }

    if(b == BI_TOP)
    {
        LOG(loggerUncategorized, Level::DEBUG, "2");
        forward = true;
        backward = false;
        return;
    }
#ifdef DYNAMIC_SYMBOLS
  if(is_dynamic_type(a)) {
    forward = backward = false;
    return; // dyntypes are only subtypes of BI_TOP
  }
  // return subtype(b, BI_STRING); // dyntypes are direct subtypes of STRING
  if(is_dynamic_type(b)) {
    forward = backward = false; // and always leaf types
    return;
  }
#endif

#define SUBTYPE_OPT    
#ifdef SUBTYPE_OPT
    // Handle the slightly complicated case of leaftypes. In PET,
    // leaftypes are recursive, so a leaftype can be a subtype of
    // another leaftype.
    if(is_leaftype(a) && is_leaftype(b)) // both types are leaftypes
    {
        LOG(loggerUncategorized, Level::DEBUG, "3");

        // Follow the leaftype_parent chain of a up to the first
        // non-leaftype or b. If we encounter b, a is a subtype of b.
        // Otherwise do the same for b.

        type_t savedA = a;
        do
        {
            a = leaftypeparent[a - first_leaftype]; 
        } while(a != b && is_leaftype(a));
        if(a == b)
        {
            forward = true;
            backward = false;
            LOG(loggerUncategorized, Level::DEBUG,
                " - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
            return;
        }
        a = savedA;
        do
        {
            b = leaftypeparent[b - first_leaftype]; 
        } while(b != a && is_leaftype(b));
        if(b == a)
        {
            backward = true;
            forward = false;
            LOG(loggerUncategorized, Level::DEBUG,
                " - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
            return;
        }
        forward = false;
        backward = false;
        LOG(loggerUncategorized, Level::DEBUG,
            " - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
        return;
    }
    else if(is_leaftype(a)) // a is a leaftype, b not
    {
        LOG(loggerUncategorized, Level::DEBUG, "4");

        backward = false; // a non-leaftype cannot be subtype of a leaftype
        do
        {
            a = leaftypeparent[a - first_leaftype]; 
        } while(is_leaftype(a));
        forward = core_subtype(a, b);
        LOG(loggerUncategorized, Level::DEBUG,
            " - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
        return;
    }
    else if(is_leaftype(b)) // b is a leaftype, a not
    {
        LOG(loggerUncategorized, Level::DEBUG, "5");
        forward = false; // a non-leaftype cannot be subtype of a leaftype
        do
        {
            b = leaftypeparent[b - first_leaftype]; 
        } while(is_leaftype(b));
        backward = core_subtype(b, a);
        LOG(loggerUncategorized, Level::DEBUG,
            " - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
        return;
    }
#else
    if(is_leaftype(a) || is_leaftype(b))
    {
        forward = subtype(a, b);
        backward = subtype(b, a);
        LOG(loggerUncategorized, Level::DEBUG,
            "456 - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
        return;
    }
#endif
    
    subset_bidir(*typecode[a], *typecode[b], forward, backward);
    LOG(loggerUncategorized, Level::DEBUG,
        "6 - %c%c", forward ? 't' : 'f', backward ? 't' : 'f');
}
#endif

#ifndef FLOP
int leaftype_parent(int t)
{
  if(is_leaftype(t))
    return leaftypeparent[t - first_leaftype];
  else
    return t;
}
#endif

int glb(int s1, int s2)
{
  if(s1 == s2) return s1;

  if(s2 < s1)
    {
      int s;
      s = s1; s1 = s2; s2 = s;
    }

  // now we know that s1 < s2

  if(s1 == BI_TOP) return s2;
  if(s1 < 0) return T_BOTTOM;

#ifndef FLOP
  // result is a _reference_ to the cache entry -> automatic writeback
  int &result = glbcache[ (typecachekey_t) s1*ntypes + s2 ];
  if(result) return result;

  if(is_leaftype(s1))
    {
      if(subtype(s1, s2))
        return (result = s1);
      else if(subtype(s2, s1))
        return (result = s2);
      else 
        return (result = -1);
    }
  else if(is_leaftype(s2))
    {
      if(subtype(s2, s1))
        return (result = s2);
      else if(subtype(s1, s2))
        return (result = s1);
      else
        return (result = -1);
    }
#else
  int result;

  if(leaftypeparent[s1] != -1)
    {
      if(subtype(s1, s2))
        return s1;
      else if(subtype(s2, s1))
        return s2;
      else
        return -1;
    }
  else if(leaftypeparent[s2] != -1)
    {
      if(subtype(s2, s1))
        return s2;
      else if(subtype(s1, s2))
        return s1;
      else
        return 1;
    }
#endif

  return (result = core_glb(s1,s2));
}
