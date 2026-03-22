#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>

#define PAGE_SIZE ((uint16_t)4096)
#define MAX_PAGES ((uint32_t)65535)

typedef struct
{
  int fd;             //.db file desc
  uint32_t num_pages; // num of pages on .db file
  void* cache;        // cache of pages (simple for now)
} Pager;

Pager* p_open(const char* filename);
int p_close(Pager* p);

int read_page(Pager* p, uint32_t page_no, void* page_data);
int write_page(Pager* p, void* page_data);
int update_page(Pager* p, uint32_t page_no, void* page_data);
int free_page(Pager* p, uint32_t page_no);

#endif