/* PET
 * Platform for Experimentation with effficient HPSG processing Techniques
 * (C) 1999 - 2001 Ulrich Callmeier uc@coli.uni-sb.de
 */

/* interface to the [incr tsdb()] system */

#ifndef _ITSDB_H_
#define _ITSDB_H_

#include <stdio.h>
#include <time.h>

#include "chart.h"

#define MICROSECS_PER_SEC 1000000

#ifdef TSDBAPI
extern "C" {
#include "itsdb.h"
}
void tsdb_mode();
void cheap_tsdb_summarize_run();
void cheap_tsdb_summarize_item(chart &, int, int, int, char * = NULL);
void cheap_tsdb_summarize_error(error &);
#endif

class statistics
{
 public:
  int id;                     /* item id */
  int readings;               /* nr of readings */
  int words;                  /* nr of words */
  int words_pruned;           /* nr of words pruned by chart manipulation */
  int first;                  /* time for first reading */
  int tcpu;                   /* total cpu time */
  int ftasks_fi;              /* filtered tasks (by rule filter) */
  int ftasks_qc;              /* filtered tasks (by quickcheck) */
  int etasks;                 /* executed tasks */
  int stasks;                 /* suceeding tasks */
  int aedges;                 /* active items in chart */
  int pedges;                 /* passive items in chart */
  int raedges;                /* active items contributing to result */
  int rpedges;                /* passive items contributing to result */
  int unifications_succ;      /* nr of successfull unifications */
  int unifications_fail;      /* nr of failed unifications */
  int copies;                 /* nr of copies */
  long dyn_bytes;             /* total dynamic memory in bytes */
  long stat_bytes;            /* total static memory in bytes */
  int cycles;                 /* cycles found */
  int fssize;                 /* avg size of all passive edges */
  int nmeanings;              /* number of well-formed semantic formulae; typically <= readings */
  int unify_cost_succ;
  int unify_cost_fail;

  void reset();
  void print(FILE *f);
};

extern statistics stats;

void initialize_version();

// this class provides a clock that starts running on construction
// elapsed() returns elapsed time since start in some unknown unit
// provides functions to convert this to milliseconds, and to get resolution
// timer can be stopped and restarted
// this implementation will fail when clock() wraps over, which happens after
// about 36 minutes on solaris/linux on 32 bit machines

class timer
{
 public:
  inline timer(bool running = true) : _start(0), _elapsed(0), _saved(0), _running(false)
    { if(running) start(); }

  inline ~timer() {};

  bool running() { return _running; }

  void start()
    { if(!_running) { _running = true; _start = clock(); } }

  void stop()
    { if(_running) { _running = false; _elapsed += clock() - _start; } }

  void save() { _saved = _elapsed; }
  void restore() { _elapsed = _saved; }

  inline clock_t elapsed() { return _elapsed + (_running ? clock() - _start : 0); }
  // returns elapsed time since start in some unknown unit
  
  inline clock_t convert2ms(clock_t t) { return t / (CLOCKS_PER_SEC / 1000); }
  // converts time in above unit to milliseconds

  inline double convert2s(clock_t t) { return convert2ms(t) / 1000.; }
  // converts time in above unit to seconds

  inline clock_t resolution() { return 1000 / CLK_TCK; }
  // returns `stepsize' of the clock in milliseconds
 private:
  clock_t _start;
  clock_t _elapsed;
  clock_t _saved;
  bool _running;
};

#endif

