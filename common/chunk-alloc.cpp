/* PET
 * Platform for Experimentation with efficient HPSG processing Techniques
 * (C) 1999 - 2001 Ulrich Callmeier uc@coli.uni-sb.de
 */

/* class to manage chunks of memory for efficient allocation of objects */

/* this class has several implementations of its low-level memory allocation,
   one that is unix dependend, supporting two heaps with disjunct and
   easy to distinguish address rooms to allow zero-overhead seperation
   of permanent and non-permant dags (colored pointers); another windows-
   specific implementation that uses VirtualAlloc; and a third one that
   relies on standard C++ memory management only */

#ifdef USEMMAP
#include <unistd.h>
#endif

#include "pet-system.h"
#include "chunk-alloc.h"
#ifdef FLOP
#include "flop.h"
#else
#include "cheap.h"
#endif

chunk_allocator t_alloc(CHUNK_SIZE, false);
chunk_allocator p_alloc(CHUNK_SIZE, true);

chunk_allocator::chunk_allocator(int chunk_size, bool down        )
{
#ifdef USEMMAP
  int pagesize = sysconf(_SC_PAGESIZE);
  _chunk_size = (((chunk_size-1)/pagesize)+1)*pagesize;
#else
  _chunk_size = chunk_size;
#endif

  _curr_chunk = 0;
  _chunk_pos = 0;
  _nchunks = 0;

  _chunk = New char* [MAX_CHUNKS];

  _init_core(down);

  _chunk[_nchunks++] = (char *) _core_alloc(_chunk_size);

  _stats_chunk_sum = _stats_chunk_n = 0;

  _max = 0;
}

chunk_allocator::~chunk_allocator()
{
  for(int i = 0; i < _nchunks; i++)
    _core_free(_chunk[i], _chunk_size);

  delete[] _chunk;
}

#ifdef __borlandc__
#pragma argsused
#endif
void chunk_allocator::_overflow(int n)
{
  if(n > _chunk_size)
    throw error("alloc: chunk_size too small");

  if(++_curr_chunk >= MAX_CHUNKS)
    throw error("alloc: out of chunks");
  
  if(_curr_chunk >= _nchunks)
    {
      _chunk[_nchunks++] = (char *) _core_alloc(_chunk_size);
    }
  
  _chunk_pos = 0;
}

void chunk_allocator::may_shrink()
{
  _stats_chunk_n++;
  _stats_chunk_sum+=_curr_chunk;

  int avg_chunks = _stats_chunk_sum / _stats_chunk_n;

  // we want to shrink if _nchunks is way above average & current usage is not above average

  if(_nchunks > avg_chunks * 2 && _curr_chunk <= avg_chunks + 1)
    {
#ifdef DEBUG
      fprintf(ferr, "shrink (avg: %d, used: %d, allocated: %d) -> ",
	      avg_chunks, _curr_chunk, _nchunks);
#endif

      while(_nchunks > (_curr_chunk * 2 + 1) && _nchunks > avg_chunks)
	{
	  _core_free(_chunk[--_nchunks], _chunk_size);
	}

#ifdef DEBUG
      fprintf(ferr, "%d\n", _nchunks);
#endif
    }
#ifdef DEBUG
  else
    {
      fprintf(ferr, "noshrink (avg: %d, used: %d, allocated: %d)\n",
	      avg_chunks, _curr_chunk, _nchunks);
    }
#endif
}

void chunk_allocator::reset()
{
  _stats_chunk_sum = _stats_chunk_n = 0;
  _curr_chunk = _chunk_pos = 0;
  _max = 0;
  may_shrink();
}

#ifdef USEMMAP

//
// core memory allocation for UNIX using mmap
//

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

// we want two heaps, one extending from low to high adresses, the
// other from high to low adresses
// linux mmap returns low to high, linux malloc low to high;
// linux mmap respects an addr argument, even without MAP_FIXED
// solaris mmap return high to low, solaris malloc low to high;
// solaris mmap doesn't respect an addr argument without MAP_FIXED

// --> strategy:
// (linux):   use mmap(0) for low to high, mmap(addr) for high to low
// (solaris): use mmap(addr) for low to high, mmap(0) for high to low

#if defined(linux)
#define _MMAP_ANONYMOUS
#define _CORE_LOW  0x50000000
#define _CORE_HIGH 0xbff00000
#else
#define _CORE_LOW  0x30000000
#define _CORE_HIGH 0xd0000000
#define _MMAP_DOWN
#endif

#ifndef _MMAP_ANONYMOUS
static int dev_zero_fd = -1;
#endif

char *_mmap_up_mark = (char *) _CORE_LOW,
     *_mmap_down_mark = (char *) _CORE_HIGH;

void chunk_allocator::_init_core(bool down)
{
#ifndef _MMAP_ANONYMOUS
  if(dev_zero_fd == -1)
    dev_zero_fd = open("/dev/zero", O_RDWR);
#endif

  _core_down = down;
}

inline void *mmap_mmap(void *addr, int size)
{
#ifndef _MMAP_ANONYMOUS
  return mmap((char *) addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|(addr == 0 ? 0 : MAP_FIXED), dev_zero_fd, 0);
#else
  return mmap(addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
}

void *chunk_allocator::_core_alloc(int size)
{
  void *p = MAP_FAILED;

#ifdef _MMAP_DOWN
  if(_core_down)
    p = mmap_mmap(0, size);
  else
    p = mmap_mmap(_mmap_up_mark, size);
#else
  if(!_core_down)
    p = mmap_mmap(0, size);
  else
    p = mmap_mmap(_mmap_down_mark - size, size);
#endif

  if(p == MAP_FAILED)
    {
      perror("chunk_allocator::_core_alloc");
      fprintf(ferr,
	      "couldn't mmap %d bytes %s (up = %xd, down = %xd)\n",
	      size,
	      _core_down ? "down" : "up",
	      (int) _mmap_up_mark, (int) _mmap_down_mark);
      throw error("alloc: mmap problem");
    }

  if(_core_down)
    _mmap_down_mark = (char *) p;
  else
    _mmap_up_mark = (char *) p + size;

  if(_mmap_up_mark >= _mmap_down_mark)
    {
      fprintf(ferr, "alloc: no space (up = %xd, down = %xd)\n",
	      (int) _mmap_up_mark, (int) _mmap_down_mark);
      throw error("alloc: out of mmap space");
    }

  return p;
}

int chunk_allocator::_core_free(void *p, int size)
{
  int res = munmap((char *) p, size);

  if(res == -1)
    throw error("alloc: munmap error");

  if(_core_down)
    _mmap_down_mark += size;
  else
    _mmap_up_mark -= size;

  return res;
}

#else

#ifdef GENERIC_CHUNKALLOC

//
// generic core memory allocation for Windows et al
//

#pragma argsused
void chunk_allocator::_init_core(bool down)
{
}

void *chunk_allocator::_core_alloc(int size)
{
  void *p = (void *) New char[size];
  return p;
}

#pragma argsused
int chunk_allocator::_core_free(void *p, int size)
{
  delete[] (char *) p;
  return 1;
}

#else

#ifdef __BORLANDC__
// Use WinAPI VirtualAlloc and VirtualFree

#include "winbase.h"

#pragma argsused
void chunk_allocator::_init_core(bool down)
{
}

void *chunk_allocator::_core_alloc(int size)
{
  void *p = (void *)
    VirtualAlloc(0, size, MEM_COMMIT, PAGE_READWRITE);
  return p;
}

#pragma argsused
int chunk_allocator::_core_free(void *p, int size)
{
  VirtualFree(p, 0, MEM_RELEASE);
  return 1;
}

#else
#error "No platform specific allocator defined"
#endif

#endif

#endif
