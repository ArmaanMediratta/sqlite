#ifndef PAGER_H
#define PAGER_H

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE  ((uint16_t)4096)
#define CACHE_SIZE ((uint16_t)64)

typedef enum
{
  PAGER_OK,
  PAGER_ERR_IO,
  PAGER_ERR_NOMEM,
  PAGER_ERR_CORRUPT
} PagerStatus;

typedef struct
{
  uint32_t page_no;
  void* data;
  bool ref;
  bool dirty;
} Page;

typedef struct
{
  int fd;                 //.db file desc
  uint32_t num_pages;     // num of pages on .db file
  Page cache[CACHE_SIZE]; // cache arr (2nd change algo)
  uint32_t clock_hand;
} Pager;

PagerStatus p_open(const char* filename, Pager** out);
PagerStatus p_close(Pager* p);

PagerStatus p_alloc_page(Pager* p, uint32_t* page_no);
PagerStatus p_read_page(Pager* p, uint32_t page_no, void** page_data);
PagerStatus p_write_page(Pager* p, uint32_t page_no, void* page_data);

#endif