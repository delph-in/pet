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

/* class `chart' */

#include "pet-system.h"
#include "cheap.h"
#include "chart.h"
#include "tsdb++.h"
#include "item-printer.h"

//#define DEBUG

chart::chart(int len, auto_ptr<item_owner> owner)
    : _Chart(), _trees(), _readings(), _pedges(0),
      _Cp_start(len + 1), _Cp_end(len + 1),
      _Ca_start(len + 1), _Ca_end(len + 1),
      _Cp_span(len + 1),
      _item_owner(owner)
{
    for(int i = 0; i <= len; i++)
    {
        _Cp_span[i].resize(len + 1 - i);
    }
}

chart::~chart()
{
}

void chart::add(tItem *it)
{
#ifdef DEBUG
    it->print(ferr);
    fprintf(ferr, "\n");
#endif

    _Chart.push_back(it);

    if(it->passive())
    {
        _Cp_start[it->start()].push_back(it);
        _Cp_end[it->end()].push_back(it);
        _Cp_span[it->start()][it->end()-it->start()].push_back(it);
        _pedges++;
    }
    else
    {
        if(it->left_extending())
            _Ca_start[it->start()].push_back(it);
        else
            _Ca_end[it->end()].push_back(it);
    }
}

/** A predicate testing the existence of some item in a hash_set */
class contained : public unary_function< bool, tItem *> {
public:
  contained(hash_set<tItem *> &the_set) : _set(the_set) { } 
  bool operator()(tItem *arg) { return _set.find(arg) != _set.end(); }
private:
  hash_set<tItem *> _set;
};

/** Remove the items in the set from the chart */
void chart::remove(hash_set<tItem *> &to_delete)
{ 
    _Chart.erase(remove_if(_Chart.begin(), _Chart.end(), contained(to_delete))
                 , _Chart.end());
    for(hash_set<tItem *>::const_iterator hit = to_delete.begin()
          ; hit != to_delete.end(); hit++) {
#ifdef DEBUG
      it->print(ferr);
      fprintf(ferr, "removed \n");
#endif
      
      tItem *it = *hit;
      if(it->passive())
        {
          _Cp_start[it->start()].remove(it);
          _Cp_end[it->end()].remove(it);
          _Cp_span[it->start()][it->end()-it->start()].remove(it);
          _pedges--;
        }
      else
        {
          if(it->left_extending())
            _Ca_start[it->start()].remove(it);
          else
            _Ca_end[it->end()].remove(it);
        }
    }
}

void chart::print(FILE *f)
{
    int i = 0;
    for(chart_iter pos(this); pos.valid(); pos++, i++)
    {
        (pos.current())->print(f);
        fprintf(f, "\n");
    }
}

void chart::print(tItemPrinter *f, bool passives, bool actives)
{
    int i = 0;
    for(chart_iter pos(this); pos.valid(); pos++, i++)
    {
      tItem *curr = pos.current();
      if ((curr->passive() && passives) || (! curr->passive() && actives))
        f->print(curr);
    }
}

void chart::get_statistics()
{
    // calculate aedges, pedges, raedges, rpedges
    chart_iter iter(this);
    
    long int totalsize = 0;
    
    while(iter.valid())
    {
        tItem *it = iter.current();
        
        if(it->trait() == INFL_TRAIT)
	{
            stats.medges++;
	}
        else if(it -> passive())
	{
            stats.pedges++;
            if(it -> result_contrib())
                stats.rpedges++;
            
            fs f = it -> get_fs();
            totalsize += f.size();
	}
        else
        {
            stats.aedges++;
            if(it -> result_contrib())
                stats.raedges++;
        }
        iter++;
    }
    stats.fssize = (stats.pedges > 0) ? totalsize / stats.pedges : 0;
}



/** Return \c true if the chart is connected using only edges considered \a
 *  valid, i.e., there is a path from the first to the last node.
 */
bool
chart::connected(item_predicate &valid) {
  vector<bool> reached(rightmost() + 1, false);
  queue<int> current;
  int pos;

  reached[0] = true;
  current.push(0);
  while(! reached[rightmost()] && ! current.empty()) {
    pos = current.front(); current.pop();
    for(list<tItem *>::iterator it = _Cp_start[pos].begin()
          ; it != _Cp_start[pos].end(); it++) {
      if (! reached[(*it)->end()] && valid(*it)) {
        reached[(*it)->end()] = true;
        current.push((*it)->end());
      }
    }
  }
  return reached[rightmost()];
}

#if 0
// This function is not really useful
/** Find uncovered regions (gaps) in the chart, i.e., regions that are not or
 *  only partially covered by items considered as \a valid.
 *
 * \param narrow if true, the regions will be as narrow as possible, meaning
 *               that a gap starts at the rightmost loose end on the left side
 *               and ends on the leftmost loose end on the right
 *               side. Otherwise, the gaps will include all partially covered
 *               regions, which leads to some strange uncovered regions, but
 *               maybe more robustness.
 */
vector<bool> 
chart::uncovered_regions(item_predicate &valid, bool narrow)
{
    if(verbosity > 4)
      fprintf(ferr, "finding uncovered regions (0 - %d):", rightmost());

    list<tItem *>::iterator it;
    // true means that at this chart position a node has zero in/out-degree
    // with respect to valid() edges
    vector<bool> zero_in(rightmost() + 1, true);
    vector<bool> zero_out(rightmost() + 1, true);
    vector<bool>::size_type pos;

    for(pos = 0; pos <= rightmost(); pos++) {
      it = _Cp_start[pos].begin();
      while((it != _Cp_start[pos].end()) && zero_out[pos]) {
        zero_out[pos] = ! valid(*it);
        it++;
      }
      it = _Cp_end[pos].begin();
      while(it != _Cp_end[pos].end() && zero_in[pos]) {
        zero_in[pos] = ! valid(*it);
        it++;
      }
    }
    
    vector<bool> uncovered(rightmost() + 1, true);

    if(narrow) {
      // special cases : start and end of chart
      uncovered[0] = zero_out[0];
      uncovered[rightmost()] = zero_in[rightmost()];
      for(pos = 1; pos < rightmost(); pos++) {
        uncovered[pos] = ((zero_out[pos] && zero_in[pos + 1])
                          || (zero_out[pos - 1] && zero_in[pos])) ;
      }
    } else {
      bool gap;
      // propagate gap first left to right, then vice versa
      gap = uncovered[0] = zero_out[0];
      for(pos = 1; pos < rightmost(); pos++) {
        if(gap) {
          // l or r dangling node: gap continues
          uncovered[pos] = gap = (zero_out[pos] || zero_in[pos]);
        } else {
          // loose end node starts gap
          uncovered[pos] = gap = (! zero_in[pos] && zero_out[pos]);
        }
      }
      gap = uncovered[rightmost()] = zero_in[rightmost()];
      for(int pos = rightmost() - 1; pos > 0; pos--) {
        if(gap) {
          // l or r dangling node: gap continues
          uncovered[pos] = gap = (zero_out[pos] || zero_in[pos]);
        } else {
          // loose start node starts gap
          uncovered[pos] = gap = (! zero_out[pos] && zero_in[pos]);
        }
      }
    }

    return uncovered;
}
#endif
