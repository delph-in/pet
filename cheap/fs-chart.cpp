/* PET
 * Platform for Experimentation with efficient HPSG processing Techniques
 * (C) 2007 Peter Adolphs
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

#include "fs-chart.h"

#include "chart.h"
#include "cheap.h"
#include "errors.h"
#include "item.h"
#include "item-printer.h"

#include <list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace HASH_SPACE;
using boost::lexical_cast;


// =====================================================
// class tChart
// =====================================================

// helper function:
/**
 * Add all \a items that are not included in \a skip to \a result.
 */
inline static void
filter_items(const item_list &items,
    bool skip_blocked,
    bool skip_pending_inflrs,
    item_list &skip,
    item_list &result)
{
  for (item_list::const_iterator it = items.begin();
       it != items.end();
       it++)
  {
    if (!(skip_blocked && (*it)->blocked())
        && !(skip_pending_inflrs && !(*it)->inflrs_complete_p())
        && (find(skip.begin(), skip.end(), *it) == skip.end()))
    {
      result.push_back(*it);
    }
  }
}

void
tChart::clear()
{
  for (std::list<tChartVertex*>::iterator it = _vertices.begin();
       it != _vertices.end();
       it++)
    delete *it;
  _vertices.clear();
  _items.clear(); // items are usually released by the tItem::default_owner
  _vertex_to_starting_items.clear();
  _vertex_to_ending_items.clear();
  _item_to_prec_vertex.clear();
  _item_to_succ_vertex.clear();
}

std::list<tChartVertex*>
tChart::start_vertices(bool connected)
{
  std::list<tChartVertex*> vertices;
  for (std::list<tChartVertex*>::iterator it = _vertices.begin();
       it != _vertices.end();
       it++)
  {
    if ((*it)->ending_items().empty() &&
        (!connected || !(*it)->starting_items().empty()))
    {
      vertices.push_back(*it);
    }
  }
  return vertices;
}

std::list<tChartVertex*>
tChart::end_vertices(bool connected)
{
  std::list<tChartVertex*> vertices;
  for (std::list<tChartVertex*>::iterator it = _vertices.begin();
       it != _vertices.end();
       it++)
  {
    if ((*it)->starting_items().empty() &&
        (!connected || !(*it)->ending_items().empty()))
    {
      vertices.push_back(*it);
    }
  }
  return vertices;
}

tItem*
tChart::add_item(tItem *item, tChartVertex *prec, tChartVertex *succ)
{
  assert((item != NULL) && (prec != NULL) && (succ != NULL));
  assert((prec->_chart == this) && (succ->_chart == this));

  _items.push_back(item);
  _item_to_prec_vertex[item] = prec;
  _item_to_succ_vertex[item] = succ;
  _vertex_to_starting_items[prec].push_back(item);
  _vertex_to_ending_items[succ].push_back(item);

  item->notify_chart_changed(this);

  return item;
}

void
tChart::remove_item(tItem *item)
{
  assert(item != NULL);
  assert(item->_chart == this);
  assert(_item_to_prec_vertex.find(item) != _item_to_prec_vertex.end());

  _vertex_to_starting_items[item->prec_vertex()].remove(item);
  _vertex_to_ending_items[item->succ_vertex()].remove(item);
  _item_to_prec_vertex.erase(item);
  _item_to_succ_vertex.erase(item);
  _items.remove(item);

  item->notify_chart_changed(0);

  // items are usually released by the tItem::default_owner
}

void
tChart::remove_items(item_list items)
{
  for (item_iter it = items.begin(); it != items.end(); it++) {
    remove_item(*it);
  }
}

item_list
tChart::items(bool skip_blocked, bool skip_pending_inflrs, item_list skip)
{
  item_list result;
  filter_items(_items, skip_blocked, skip_pending_inflrs, skip, result);
  return result;
}

item_list
tChart::same_cell_items(tItem *item, bool skip_blocked,
    bool skip_pending_inflrs, item_list skip)
{
  item_list result;
  filter_items(item->prec_vertex()->starting_items(), skip_blocked,
      skip_pending_inflrs, skip, result);
  return result;
}

item_list
tChart::succeeding_items(tItem *item, bool skip_blocked,
    bool skip_pending_inflrs, item_list skip)
{
  item_list result;
  filter_items(item->succ_vertex()->starting_items(), skip_blocked,
      skip_pending_inflrs, skip, result);
  return result;
}

item_list
tChart::all_succeeding_items(tItem *item, bool skip_blocked,
    bool skip_pending_inflrs, item_list skip)
{
  item_list result;
  std::list<tChartVertex*> vertices;
  vertices.push_back(item->succ_vertex());
  for (std::list<tChartVertex*>::iterator vit = vertices.begin();
       vit != vertices.end();
       vit++)
  {
    item_list succ_items = (*vit)->starting_items();
    // schedule processing of all vertices not processed before:
    for (item_iter iit = succ_items.begin(); iit != succ_items.end(); iit++) {
      tChartVertex *succ = (*iit)->succ_vertex();
      if (find(vertices.begin(), vertices.end(), succ) == vertices.end())
        vertices.push_back(succ);
    }
    // add filtered succeeding items to the result list:
    filter_items(succ_items, skip_blocked, skip_pending_inflrs, skip, result);
  }
  return result;
}

void
tChart::print(std::ostream &out, tAbstractItemPrinter *printer,
              bool passives, bool actives)
{
  tItemPrinter default_printer(out, verbosity > 2, verbosity > 10);
  if (printer == NULL) {
    printer = &default_printer;
  }
  item_list all_items = items();
  for (item_iter it = all_items.begin(); it != all_items.end(); it++) {
    tItem *item = *it;
    if ((item->passive() && passives) || (! item->passive() && actives)) {
      printer->print(item); out << endl;
    }
  }
}


/* helper function for tChartUtil::map_chart() */
inline static tChartVertex*
retrieve_chart_vertex(tChart &chart,
    hash_map<int,tChartVertex*> &vertex_map,
    int position)
{
  tChartVertex *v = vertex_map[position];
  if (v == 0) {
    v = chart.add_vertex(tChartVertex::create());
    vertex_map[position] = v;
  }
  return v;
}



// =====================================================
// class tChartUtil
// =====================================================

// helper function:
static bool
init_lpath(list_int*& lpath, string name, string &errors)
{
  free_list(lpath);
  char *val = cheap_settings->value(name.c_str());
  lpath = path_to_lpath(val);
  if (val && (lpath == 0)) {
    errors.append("Invalid path in setting `" + name + "'.\n");
  }
  return val && lpath;
}

// define static members:
list_int* tChartUtil::_token_form_path = 0;
list_int* tChartUtil::_token_id_path = 0;
list_int* tChartUtil::_token_from_path = 0;
list_int* tChartUtil::_token_to_path = 0;
list_int* tChartUtil::_token_stem_path = 0;
list_int* tChartUtil::_token_lexid_path = 0;
list_int* tChartUtil::_token_inflr_path = 0;
list_int* tChartUtil::_token_postags_path = 0;
list_int* tChartUtil::_token_posprobs_path = 0;
list_int* tChartUtil::_lexicon_tokens_path = 0;
list_int* tChartUtil::_context_path = 0;
list_int* tChartUtil::_input_path = 0;
list_int* tChartUtil::_output_path = 0;
list_int* tChartUtil::_poscons_path = 0;

void
tChartUtil::initialize()
{
  string e; // errors
  string w; // warnings
  string m; // missing features for interpreting input fs

  if (!init_lpath(_token_form_path, "token-form-path", e))
    e += "Setting `token-form-path' required for interpreting input fs.\n";
  if (!init_lpath(_token_id_path, "token-id-path", e))
    m += "item ids, ";
  if (!init_lpath(_token_from_path, "token-from-path", e))
    m += "surface start positions, ";
  if (!init_lpath(_token_to_path, "token-to-path", e))
    m += "surface end positions, ";
  if (!init_lpath(_token_stem_path, "token-stem-path", e))
    m += "stems, ";
  if (!init_lpath(_token_lexid_path, "token-lexid-path", e))
    m += "lexical ids, ";
  if (!init_lpath(_token_inflr_path, "token-inflr-path", e))
    m += "inflectional rules, ";
  if (   !init_lpath(_token_postags_path, "token-postags-path", e)
      || !init_lpath(_token_posprobs_path, "token-posprobs-path", e))
    m += "parts of speech, ";
  if (!init_lpath(_lexicon_tokens_path, "lexicon-tokens-path", e))
    w += " [ no input fs in lexical items ] ";
  if (!init_lpath(_context_path, "chart-mapping-context-path", e))
    e += "Setting `mapping-context-path' required for chart mapping.\n";
  if (!init_lpath(_input_path, "chart-mapping-input-path", e))
    e += "Setting `mapping-input-path' required for chart mapping.\n";
  if (!init_lpath(_output_path, "chart-mapping-output-path", e))
    e += "Setting `mapping-output-path' required for chart mapping.\n";
  if (!init_lpath(_poscons_path, "chart-mapping-position-path", e))
    e += "Setting `mapping-position-path' required for chart mapping.\n";

  if (!m.empty() && verbosity > 4) {
    m.erase(m.end()-2, m.end()); // erase last ", " from list of missing tags
    w += " [ no input fs mapping for " + m + " ] ";
  }
  if (!w.empty())
    fprintf(ferr, "%s", w.c_str());
  if (!e.empty())
    throw tError(e);
}

tInputItem*
tChartUtil::create_input_item(const fs &input_fs)
{
  if (!input_fs.valid())
    throw tError("Invalid token feature structure.");

  string id; // the external id (a concatenation of the ids from input_fs)
  string form;
  string stem;
  int cfrom = -1;
  int cto = -1;
  int vfrom = -1;
  int vto = -1;
  int token_class = WORD_TOKEN_CLASS; // default; maybe reset in the following
  list<type_t> infls; // inflectional rules to be applied if STEM_TOKEN_CLASS
  postags tagsnprobs; // part-of-speech tags and probabilities

  // extract information from input_fs:
  type_t form_type = input_fs.get_path_value(_token_form_path).type();
  if (form_type != BI_STRING) {
    form = get_printname(form_type);
  }
  if (_token_id_path) {
    list<fs> ids_list = input_fs.get_path_value(_token_id_path).get_list();
    for (list<fs>::iterator it = ids_list.begin();
         it != ids_list.end();
         it++)
      id += (std::string)(id.empty() ? "" : ",") + (*it).printname();
  }
  if (_token_from_path) {
    try {
      cfrom = boost::lexical_cast<int>(
          input_fs.get_path_value(_token_from_path).printname());
    } catch(boost::bad_lexical_cast &error) {
      // should only happen if the value is not set. ignore!
    }
  }
  if (_token_to_path) {
    try {
      cto = boost::lexical_cast<int>(
          input_fs.get_path_value(_token_to_path).printname());
    } catch(boost::bad_lexical_cast &error) {
      // should only happen if the value is not set. ignore!
    }
  }
  if (_token_stem_path) {
    type_t t = input_fs.get_path_value(_token_stem_path).type();
    if (t != BI_STRING) { // if the stem is set to a string literal
      stem = get_printname(t);
      if (token_class != WORD_TOKEN_CLASS)
        throw tError("Tried to use both STEM and LEXID for the same token.");
      token_class = STEM_TOKEN_CLASS;
    }
  }
  if (_token_lexid_path) {
    type_t t = input_fs.get_path_value(_token_lexid_path).type();
    if (t != BI_STRING) { // if the lexid is set to a string literal
      string lexid = get_printname(t);
      if (token_class != WORD_TOKEN_CLASS)
        throw tError("tried to use both STEM and LEXID for the same token.");
      token_class = lookup_type(lexid);
      if (token_class == T_BOTTOM)
        throw tError((std::string) "Unknown lex-id '" + lexid + "'");
    }
  }
  if (_token_inflr_path) {
    type_t t = input_fs.get_path_value(_token_inflr_path).type();
    if (t != BI_STRING) { // for now, comma-separated list of typenames
      std::istringstream iss(get_printname(t));
      std::string rulename;
      while (std::getline(iss, rulename, ',')) {
        type_t infl_rule_type = lookup_type(rulename);
        if (infl_rule_type == T_BOTTOM)
          throw tError((std::string) "Unknown infl type '" + rulename + "'.");
        infls.push_back(infl_rule_type);
      }
    }
  }
  if (_token_postags_path && _token_posprobs_path) {
    list<fs> tags = input_fs.get_path_value(_token_postags_path).get_list();
    list<fs> probs = input_fs.get_path_value(_token_posprobs_path).get_list();
    if (tags.size() != probs.size())
      throw tError("Part-of-speech tag and probability lists are not aligned.");
    list<fs>::iterator ti, pi;
    for (ti = tags.begin(), pi = probs.begin();
         (ti != tags.end()) && (pi != probs.end());
         ti++, pi++)
    {
      if (is_string_instance(ti->type()) && is_string_instance(pi->type())) {
        string tag = (*ti).printname();
        try {
          double prob = lexical_cast<double>((*pi).printname());
          tagsnprobs.add(tag, prob);
        } catch (boost::bad_lexical_cast) {
          cerr << "Warning: Provided part-of-speech probability is not "
               << "a double (`" << (*ti).printname() << "'). Ignoring." << endl;
        }
      } else {
        cerr << "Warning: Provided part-of-speech tags and/or probabilities "
                "are no string literals. Ignoring." << endl;
      }
    }
  }

  tInputItem *item = new tInputItem(id, vfrom, vto, cfrom, cto, form, stem,
      tPaths(), token_class, modlist(), input_fs);

  if (!infls.empty()) {
    if (token_class == WORD_TOKEN_CLASS) {
      throw tError("infl rule type specified although neither a stem nor "
          "a lex-id have been specified");
    }
    item->set_inflrs(infls);
  }
  item->set_in_postags(tagsnprobs);

  return item;
}

fs
tChartUtil::create_input_fs(tInputItem* item)
{
  fs input_fs(_token_form_path, retrieve_string_instance(item->orth()));
  if (!input_fs.valid())
    throw tError("failed to create input fs");
  if (false && _token_id_path) {
    //
    // _fix_me_
    // for rules to append ID values (when multiple tokens are merged), we need
    // to make this value a difference list.  in general, i guess, one should
    // check the unify() results below, as we ran into a segmentation error in
    // the ERG when the value constraint on +ID was incompatible with BI_CONS.
    //                                                         (30-aug-08; oe)
    fs id_f = retrieve_string_instance(item->external_id());
    fs ids_f = fs(BI_CONS);
    ids_f = unify(ids_f, ids_f.get_attr_value(BIA_FIRST), id_f);
    input_fs = unify(input_fs,input_fs.get_path_value(_token_id_path),ids_f);
  }
  if (_token_from_path) {
    fs f = retrieve_string_instance(item->startposition());
    input_fs = unify(input_fs, input_fs.get_path_value(_token_from_path), f);
  }
  if (_token_to_path) {
    fs f = retrieve_string_instance(item->endposition());
    input_fs = unify(input_fs, input_fs.get_path_value(_token_to_path), f);
  }
  if (_token_stem_path) {
    string stem = item->stem();
    if (!stem.empty()) {
      fs f = retrieve_string_instance(item->stem());
      input_fs = unify(input_fs, input_fs.get_path_value(_token_stem_path),f);
    }
  }
  if (_token_lexid_path) {
    type_t t = item->tclass();
    if ((t != WORD_TOKEN_CLASS) && (t != STEM_TOKEN_CLASS)) {
      fs f = retrieve_string_instance(get_typename(t));
      input_fs = unify(input_fs,input_fs.get_path_value(_token_lexid_path),f);
    }
  }
  if (_token_inflr_path) {
    list_int *infl_l = item->inflrs();
    string infl_s;
    while (infl_l) {
      infl_s += (string)(infl_s.empty() ? "" : ",") + get_typename(infl_l->val);
      infl_l = infl_l->next;
    }
    if (!infl_s.empty()) {
      fs f = retrieve_string_instance(infl_s);
      input_fs = unify(input_fs,input_fs.get_path_value(_token_inflr_path),f);
    }
  }
  if (_token_postags_path && _token_posprobs_path) {
    // TODO I need fs::create_list(std::list<fs>)
    fs tags_f(BI_LIST);
    fs probs_f(BI_LIST);
    std::vector<std::string> tags;
    std::vector<double> probs;
    item->get_in_postags().tagsnprobs(tags, probs);
    std::vector<std::string>::iterator tags_it;
    std::vector<double>::iterator probs_it;
    list_int *path = 0;
    for (tags_it = tags.begin(), probs_it = probs.begin();
         tags_it != tags.end();
         tags_it++, probs_it++)
    {
      fs tag_f = fs(retrieve_string_instance(*tags_it));
      fs prob_f = fs(retrieve_string_instance(lexical_cast<string>(*probs_it)));
      fs tag_cons(BI_CONS);
      fs prob_cons(BI_CONS);
      tag_cons = unify(tag_cons, tag_cons.get_attr_value(BIA_FIRST), tag_f);
      prob_cons = unify(prob_cons, prob_cons.get_attr_value(BIA_FIRST), prob_f);
      if (path == 0) {
        tags_f = unify(tags_f, tags_f, tag_cons);
        probs_f = unify(probs_f, probs_f, prob_cons);
      } else {
        tags_f = unify(tags_f, tags_f.get_path_value(path), tag_cons);
        probs_f = unify(probs_f, probs_f.get_path_value(path), prob_cons);
      }
      path = append(path, BIA_REST);
    }
    fs nil(BI_NIL);
    tags_f = unify(tags_f, tags_f.get_path_value(path), nil);
    probs_f = unify(probs_f, probs_f.get_path_value(path), nil);
    free_list(path);
    fs f = fs(_token_postags_path, BI_LIST);
    input_fs = unify(input_fs, input_fs, f);
    f = fs(_token_posprobs_path, BI_LIST);
    input_fs = unify(input_fs, input_fs, f);
    input_fs = unify(input_fs, input_fs.get_path_value(_token_postags_path),
        tags_f);
    input_fs = unify(input_fs, input_fs.get_path_value(_token_posprobs_path),
        probs_f);
  }

  if (!input_fs.valid()) {
    throw tError("Construction of input feature structure for token "
        "\"" + item->orth() + "\" (external id \"" + item->external_id() +
        "\") failed.");
  }

  return input_fs;
}

/**
 * Establishes a topological order, mapping chart vertices to int values
 * indicating the precedence of a vertex (vertices with higher values come
 * before vertices with lower values).
 * \pre exactly one start and end node & no loops in the chart
 */
static void
topological_order(tChartVertex *vertex, int &max_value,
    map<tChartVertex*, int, greater<void*> > &order,
    list<tChartVertex*> &ordered)
{
  list<tItem*> items = vertex->starting_items();
  for (list<tItem*>::iterator it = items.begin(); it != items.end(); it++) {
    tChartVertex *succ = (*it)->succ_vertex();
    if (order.find(succ) == order.end()) // if not visited
      topological_order(succ, max_value, order, ordered);
  }
  max_value++;
  order[vertex] = max_value;
  ordered.push_front(vertex);
}

int
tChartUtil::assign_int_nodes(tChart &chart, item_list &processed)
{
  processed.clear();
  std::list<tChartVertex*> vertices = chart.start_vertices();
  if (vertices.size() > 1) // TODO how do we deal with several start vertices?
    fprintf(ferr, "warning: only using the first start vertex.\n");
  int max_value = -1; // will be set accordingly by topological_order()
  map<tChartVertex*, int, greater<void*> > order;
  list<tChartVertex*> ordered;
  topological_order(vertices.front(), max_value, order, ordered);
  list<tChartVertex*>::iterator vit;
  for (vit = ordered.begin(); vit != ordered.end(); vit++) {
    tChartVertex *prec_vertex = *vit;
    list<tItem*> items = prec_vertex->starting_items();
    for (list<tItem*>::iterator it = items.begin(); it != items.end(); it++) {
      tItem *item = *it;
      item->set_start(max_value - order[prec_vertex]);
      item->set_end(max_value - order[item->succ_vertex()]);
      processed.push_back(item);
    }
  }
  return max_value;
}

void
tChartUtil::map_chart(inp_list &input_items, tChart &chart)
{
  chart.clear();
  hash_map<int, tChartVertex*> vertex_map; // maps char positions to vertices

  // convert each input item to a token feature structure:
  for (list<tInputItem*>::iterator item_it = input_items.begin();
       item_it != input_items.end();
       item_it++)
  {
    // determine start and end positions (int vertices):
    int vfrom = (*item_it)->start();         // start vertex as an int
    int vto   = (*item_it)->end();           // end vertex as an int
    int cfrom = (*item_it)->startposition(); // external character position
    int cto   = (*item_it)->endposition();   // external character position

    // fix vfrom and vto in case that the position_mapper has not performed its
    // job yet:
    if (vfrom == -1 || vto == -1) {
      // TODO check: is this ok in all situations?
      vfrom = cfrom;
      vto = cto;
    }

    // get a chart vertex for the from and to positions:
    tChartVertex *from_v = retrieve_chart_vertex(chart, vertex_map, vfrom);
    tChartVertex *to_v   = retrieve_chart_vertex(chart, vertex_map, vto);

    // finally insert a new chart item into the chart
    chart.add_item(*item_it, from_v, to_v);
  }
}

int
tChartUtil::map_chart(tChart &chart, inp_list &input_items)
{
  item_list items;
  int nr_processed = tChartUtil::assign_int_nodes(chart, items);
  input_items.clear();
  for (item_iter it=items.begin(); it!=items.end(); it++) {
    tInputItem *inp_item = dynamic_cast<tInputItem*>(*it);
    assert(inp_item);
    input_items.push_back(inp_item);
  }
  return nr_processed;
}

void
tChartUtil::map_chart(chart &in, tChart &out)
{
  hash_map<int,tChartVertex*> vertex_map;
  for (chart_iter it(in); it.valid(); it++) {
    tItem *item = it.current();
    tChartVertex* prec = retrieve_chart_vertex(out, vertex_map, item->start());
    tChartVertex* succ = retrieve_chart_vertex(out, vertex_map, item->end());
    out.add_item(item, prec, succ);
  }
}

int
tChartUtil::map_chart(tChart &in, chart &out)
{
  item_list items;
  int nr_processed = tChartUtil::assign_int_nodes(in, items);
  out.reset(nr_processed);
  for (item_iter it=items.begin(); it!=items.end(); it++) {
    out.add(*it);
  }
  return nr_processed;
}
