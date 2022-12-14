/* -*- Mode: C++ -*- */
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

/** \file grammar.h
 * grammar rules, grammar
 */

#ifndef _GRAMMAR_H_
#define _GRAMMAR_H_

#include "pet-config.h"

#include "list-int.h"
#include "utility.h"
#include "types.h"
#include "fs.h"
#include "lexicon.h"
#include "trigram.h"

#include <string>
#include <list>
#include <map>
#include <ostream>
#include <cassert>

/** The different traits of rules and items:
    -- INPUT_TRAIT for items: an input item
                   for rules: n/a
    -- INFL_TRAIT  for items: n/a
                   for rules: a lexical rule with morphographemic conditions
    -- LEX_TRAIT   for items: a lexical item with all morphograph. rules applied
                   for rules: a lexical rule without morphographemic conditions
    -- SYNTAX_TRAIT a phrasal item or rule
    -- PCFG_TRAIT a pcfg item or rule
 */
enum rule_trait { SYNTAX_TRAIT, LEX_TRAIT, INFL_TRAIT, INPUT_TRAIT, PCFG_TRAIT };
enum affix_trait { NONE, PREFIX, INFLECTION, SUFFIX };

/** A rule from the HPSG grammar */
class grammar_rule
{
 public:
  /** Constructor for grammar rules.
   *  \return If the feature structure of the given type is not a valid rule
   *  (no or empty \c ARGS path), this method returns \c NULL, a grammar rule
   *  for the given type otherwise.
   */
  static grammar_rule* make_grammar_rule(type_t t);

  /** Contstructor for PCFG rules */
  static grammar_rule* make_pcfg_grammar_rule(std::vector<type_t> v);

  /** destructor */
  ~grammar_rule();

  /** Get the arity (length of right hand side) of the rule */
  inline int arity() const { return _arity; }
  /** Get the next arg that should be filled.
   *  \attn Argument numbers start at one, not at zero.
   */
  inline int nextarg() const { return first(_tofill); }
  /** Does the rule extend to the left or to the right?
   * \todo Remove the current restriction to binary rules.
   */
  inline bool left_extending() { return first(_tofill) == 1; }

  /** Get the type of this rule (instance) */
  inline int type() const { return _type; }
  /** Get the external name */
  inline const char *printname() const { return print_name(_type); }
  /** Get the unique rule id, also used in the rule filter. */
  inline int id() const { return _id; }
  /** Get the trait of this rule: INFL_TRAIT, LEX_TRAIT, SYNTAX_TRAIT */
  inline rule_trait trait() const { return _trait; }
  /** Set the trait of this rule: INFL_TRAIT, LEX_TRAIT, SYNTAX_TRAIT */
  inline void trait(rule_trait t) { _trait = t; }

  inline affix_trait affix_type() const { return _affix_type; }
  inline void set_affix_type(affix_trait type) { _affix_type = type; }

  /** Print in readable form for debugging purposes */
  void print(std::ostream &out) const;

  /** Dump grammar in a format feasible for LUI (?) into \a directory */
  // \todo should go in favor of the print method above
  void lui_dump(const char* directory = "/tmp");

  /** Return the feature structure associated with this rule.
   * If packing is active, the structure is restricted.
   */
  fs instantiate(bool full = false);

  /** Return the feature structure corresponding to the next argument */
  inline fs nextarg(const fs &f) const { return f.nth_arg(first(_tofill)); }

  /** Return the type of the next argument in PCFG rule */
  inline type_t nextarg_pcfg() { return _pcfg_args[first(_tofill) - 1]; }

  /** Return all of the arguments but the current one in the order in which
   *  they should be filled.
   */
  inline list_int *restargs() { return rest(_tofill); }

  /** Return all of the arguments in the order in which they should be
   *  filled.
   */
  inline list_int *allargs() { return _tofill; }

  /** Return the quick check vector for argument \a arg */
  inline const qc_vec &qc_vector_unif(int arg) {
    return _qc_vector_unif[arg - 1];
  }

  /** Should this rule be treated special when using hyperactive parsing?
   *  Rules whose active items are seldom reused should be made hyperactive
   *  because one dag copying operation is much more expensive than several
   *  unsuccessful unifications.
   */
  inline bool hyperactive() { return _hyper; }

  /** Return \c true if the items using this rule should always span the whole
   *  chart
   */
  inline bool spanningonly() { return _spanningonly; }

  /** Return the type of the nth argument in the pcfg rule */
  inline type_t nth_pcfg_arg(int n) { return _pcfg_args[n - 1]; }

  /** @name Rule Statistics
   * How many active (incomplete) and passive (complete)
   * edges were produced using this rule.
   */
  /*@{*/
  int actives, passives;
  /*@}*/

 private:
  grammar_rule(type_t t);
  grammar_rule(std::vector<type_t> v);
  grammar_rule() { }

  int _id;
  int _type;        // type index
  rule_trait _trait;
  affix_trait _affix_type;
  int _arity;
  list_int *_tofill;
  std::vector<type_t> _pcfg_args;

  fs _f_restriced;  // The feature structure corresponding to this rule
                    // with the packing restrictor applied.

  qc_vec *_qc_vector_unif;
  void init_qc_vector_unif();

  bool _hyper;
  bool _spanningonly;

  friend class tGrammar;
};

inline std::ostream &operator<<(std::ostream &o, const grammar_rule &g) {
  g.print(o); return o;
}

typedef std::list<grammar_rule *> rulelist;
typedef std::list<grammar_rule *>::const_iterator ruleiter;

class rulefilter {
private:
  int _nrules;
  char * _filtermatrix;

  inline char * access(grammar_rule *mother, grammar_rule *daughter) {
    assert(valid() && daughter->id() < _nrules && mother->id() < _nrules);
    return _filtermatrix + daughter->id() + _nrules * mother->id();
  }

public:
  /** create a rulefilter */
  rulefilter() : _nrules(0), _filtermatrix(0) { }

  void resize(int n) {
    _nrules = n;
    delete _filtermatrix;
    _filtermatrix = new char[n * n];
    memset(_filtermatrix, 0, n * n);
  }

  ~rulefilter() { delete[] _filtermatrix; }

  /** \return \c true only if this filter is initialized properly */
  inline bool valid() { return (_filtermatrix != NULL); }

  /** is \a daughter compatible with \a mother in the \a arg'th (1..8)
   *  argument position
   */
  inline bool get(grammar_rule * mother, grammar_rule * daughter, int arg) {
    return ! valid() || (0 != (*access(mother, daughter) & (1 << (arg - 1)))) ;
  }

  /** is \a daughter compatible with \a mother */
  inline bool get(grammar_rule * mother, grammar_rule * daughter) {
    return ! valid() || (0 != *access(mother, daughter)) ;
  }

  /** specify that \a daughter is compatible with \a mother in the \a arg'th
   *  (1..8) argument position
   */
  inline void set(grammar_rule * mother, grammar_rule * daughter, int arg) {
    char *byte = access(mother, daughter);
    *byte = *byte | (char) (1 << (arg - 1));
  }

  /** specify that \a daughter is compatible with \a mother */
  inline void set(grammar_rule * mother, grammar_rule * daughter) {
    char *byte = access(mother, daughter);
    *byte = (char) 1;
  }

  std::ostream &print(std::ostream &out) {
    out << std::endl;
    for (int i = 0; i < _nrules; ++i) {
      for (int j = 0; j < _nrules; ++j) {
        out << " " << (char)('0' + _filtermatrix[j + _nrules * i]);
      }
      out << std::endl;
    }
    return out;
  }
};

/** Class containing all the infos from the HPSG grammar */
class tGrammar {
public:

  /** Initalize this grammar by reading it from the file \a filename */
  tGrammar(const char *filename);
  ~tGrammar();

  /** \brief Look up a grammar property with name \a key. If it does not exist,
   *  the returned string will be empty.
   */
  std::string property(std::string key);

  /** Return the map containing the grammar properties */
  inline std::map<std::string, std::string> &properties()
    { return _properties; }

  /** Check if \a fs is compatible with one of the root nodes.
   *
   * \return \c true if \a f is compatible with one of the root FS nodes and
   * return the type of this root node in \a rule.
   */
  bool root(const fs &f, type_t &rule) const;
  bool root(int type) const;

  /** Return the list of attributes that should be deleted in the root dags of
   *  passive items.
   */
  list_int *deleted_daughters() const { return _deleted_daughters; }

  /** Return the packing restrictor for this grammar */
  const class restrictor &packing_restrictor() const {
    return *_packing_restrictor;
  }

  /** Is the static rule filter useable? */
  inline bool filter() { return _filter.valid(); }

  /** Is successful unification of \a daughter as \a arg th argument of \a
   *  mother possible?
   * This is a precomputed table containing all the possible rule combinations,
   * which can be rapidly looked up to avoid failing unifications.
   */
  inline bool filter_compatible(grammar_rule *mother, int arg,
                                grammar_rule *daughter) {
    return ! _filter.valid()
      || (daughter == NULL) || _filter.get(mother, daughter, arg);
  }

  /** Is rule \a a more general than \a b or vice versa?
   * If \a a or \a b are NULL, \a forward and \a backward are set to \c true,
   * otherwise, both are looked up in a precomputed table. This is a similar
   * approach to the unification rule filter.
   */
  inline void subsumption_filter_compatible(grammar_rule *a, grammar_rule *b,
                                            bool &forward, bool &backward) {
      if(!_subsumption_filter.valid() || a == 0 || b == 0) {
          forward = backward = true;
      } else {
        forward = _subsumption_filter.get(b, a);
        backward = _subsumption_filter.get(a, b);
      }
  }

  /** Return the number of rules in this grammar */
  inline const rulelist &rules() { return _rules; }
  /** Return list of lexical rules in this grammar */
  inline const rulelist &lexrules() { return _lex_rules; }

  /** Return list of PCFG rubust parsing rules in this grammar */
  inline rulelist &pcfg_rules() { return _pcfg_rules; }

  /** Return the number of hyperactive rules in this grammar */
  int nhyperrules();

  /** Return the number of stem entries in the grammar */
  inline int nstems() { return _lexicon.size(); }
  /** return a pointer to the lex_stem with type id \a inst_key, or NULL if it
   *  does not exist.
   */
  lex_stem *find_stem(type_t inst_key);
  /** return all lex_stem pointers for the base form \a s. */
  std::list<lex_stem *> lookup_stem(std::string s);

#ifdef EXTDICT
  extDictionary *extDict() { return _extDict; }
  void clear_dynamic_stems();
#endif

  // _fix_me_ becomes obsolete when yy.cpp does
  //std::list<full_form> lookup_form(const std::string form);

  /** Return the list of generic lexicon entries in this grammar */
  list_int *generics() { return _generics; }

  /** Return the list of predictor lexicon entries in this grammar*/
  list_int *predicts() { return _predicts; }

  /** \todo becomes obsolete when yy.cpp does */
  bool punctuationp(const std::string &s);

  /** Return the statistic maxent model of this grammar */
  inline class tSM *sm() { return _sm; }

  inline void sm(tSM* m) { _sm = m; }

  /** Return the lexical type predictor ME model */
  inline class tSM *lexsm() { return _lexsm; }

  /** Return the PCFG scoring model for robust parsing */
  inline class tSM *pcfgsm() { return _pcfgsm; }

  /** Return the generative model for agenda manipulation */
  inline class tGM *gm() { return _gm; }

  /** Return the lexical pruning trigram model */
  inline class tTrigramModel *lpsm() { return _lpsm; }

  /** deactivate all rules */
  void deactivate_all_rules() {
    _rules.clear();
  }

  /** activate all (and only) lexical and inflection rules */
  void activate_lex_rules() {
    deactivate_all_rules();
    _rules.insert(_rules.end(), _lex_rules.begin(), _lex_rules.end());
  }

  /** activate syntactic rules only */
  void activate_syn_rules() {
    deactivate_all_rules();
    _rules.insert(_rules.end(), _syn_rules.begin(), _syn_rules.end());
  }

  /** activate all available rules */
  void activate_all_rules() {
    activate_lex_rules();
    _rules.insert(_rules.end(), _syn_rules.begin(), _syn_rules.end());
  }

  /** Return the grammar_rule pointer for type \a type or NULL, if not
   *  available.
   */
  grammar_rule * find_rule(type_t type) {
    std::map<type_t, grammar_rule *>::iterator it = _rule_dict.find(type);
    if(it != _rule_dict.end()) {
      return it->second;
    } else {
      return NULL;
    }
  }

  /** Return the list of all loaded token mapping rules.
   * \see chart-mapping.h */
  const std::list<class tChartMappingRule*> &tokmap_rules() {
    return _tokmap_rules;
  }
  /** Return the list of all loaded lexical filtering rules.
   * \see chart-mapping.h*/
  const std::list<class tChartMappingRule*> &lexflt_rules() {
    return _lexfil_rules;
  }

 private:
  std::map<std::string, std::string> _properties;

  std::map<type_t, lex_stem *> _lexicon;
  std::multimap<std::string, lex_stem *> _stemlexicon;

#ifdef EXTDICT
  extDictionary *_extDict;
  std::list<lex_stem *> _dynamicstems;
  int _extdict_discount;
#endif

  /** The list of currently active rules.
   * If the parser is used to complete lexical processing first, only infl and
   * lex rules will be active in the first phase. Only syn rules will then be
   * active in the second phase.
   */
  rulelist _rules;

  /** The set of syntactic rules */
  rulelist _syn_rules;
  /** The set of lexical rules */
  rulelist _lex_rules;

  /** The list of pcfg robust rules */
  rulelist _pcfg_rules;

  /** Map the rule type back to the rule structure */
  std::map<type_t, grammar_rule *> _rule_dict;

  /** The set of input mapping rules */
  std::list<class tChartMappingRule*> _tokmap_rules;
  /** The set of lexical mapping rules */
  std::list<class tChartMappingRule*> _lexfil_rules;

  list_int *_root_insts;

  /** Generic lexicon entries, selected via POS tags in case no appropriate
   *  lexicon entry is found
   */
  list_int *_generics;

  /** Prediction lexicon entries, selected via lexical type prediction
      model
   */
  list_int *_predicts;

  rulefilter _filter;
  rulefilter _subsumption_filter;
  void initialize_filter();

  list_int *_deleted_daughters;
  class restrictor *_packing_restrictor;

  /// Stochastic model.
  class tSM *_sm;

  // Lexical type predictor model.
  class tSM *_lexsm;

  // Robust PCFG parsing model.
  class tSM *_pcfgsm;

  // Generative model for agenda manipulation.
  class tGM *_gm;

  // Trigram model for lexical pruning
  class tTrigramModel *_lpsm;

  void undump_properties(dumper *f);
  void init_parameters();

  friend class tGrammarUpdate;

};

class tGrammarUpdate {
public:
  tGrammarUpdate(tGrammar *, std::string &);
  ~tGrammarUpdate();

private:
  tGrammar *_grammar;
  list_int *_original_roots;
  settings *_update;

}; // class tGrammarUpdate

#endif
