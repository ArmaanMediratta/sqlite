#ifndef PAGER_H
#define PAGER_H

#include "constants.h"
#include "journal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  PAGER_OK,
  PAGER_ERR_IO,
  PAGER_ERR_NO_MEM,
  PAGER_ERR_CORRUPT,
  PAGER_ERR_BAD_PAGER,
  PAGER_ERR_INVALID_PAGE,
  PAGER_ERR_EXTERNAL
} PagerStatus;

typedef struct Page
{
  uint32_t page_no;
  void* data;
  bool ref;
  bool dirty;
  bool was_dirty;
} Page;

typedef struct Pager
{
  int fd;                  //.db file desc
  uint32_t num_pages;      // num of pages on .db file
  Page* cache[CACHE_SIZE]; // cache arr (2nd change algo)
  uint16_t num_cached;
  uint32_t clock_hand;
  Journal* j;
} Pager;

PagerStatus p_open(Pager** out, const char* filename);
PagerStatus p_close(Pager* p);

PagerStatus p_alloc_page(Pager* p, uint32_t* page_no);
PagerStatus p_read_page(Pager* p, uint32_t page_no, void** page_data);
PagerStatus p_write_page(Pager* p, uint32_t page_no, void* page_data);
PagerStatus p_commit(Pager* p);

#endif