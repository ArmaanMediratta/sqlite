#include "pager.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static int load(Pager* p, Page* page)
{
  return pread(p->fd, page->data, PAGE_SIZE, page->page_no * PAGE_SIZE);
}

static int flush(Pager* p, Page* page)
{
  return pwrite(p->fd, page->data, PAGE_SIZE, page->page_no * PAGE_SIZE);
}

static PagerStatus create_page(Page** page, uint32_t page_no)
{
  Page* pg = calloc(1, sizeof(Page));
  if (pg == NULL)
    return PAGER_ERR_NO_MEM;

  pg->data = calloc(1, PAGE_SIZE);
  if (pg->data == NULL)
  {
    free(pg);
    return PAGER_ERR_NO_MEM;
  }

  pg->page_no = page_no;
  *page       = pg;

  return PAGER_OK;
}

static uint32_t find_victim(Pager* p)
{
  if (p->num_cached == CACHE_SIZE)
  {
    while (true)
    {
      Page* pg = p->cache[p->clock_hand];

      if (pg->ref)
      {
        pg->ref       = false;
        p->clock_hand = (p->clock_hand + 1) % CACHE_SIZE;
      }
      else if (pg->dirty)
      {
        pg->dirty     = false;
        p->clock_hand = (p->clock_hand + 1) % CACHE_SIZE;
      }
      else
      {
        if (pg->was_dirty && flush(p, pg) != PAGE_SIZE)
          return UINT32_MAX;

        return (p->clock_hand++) % CACHE_SIZE;
      }
    }
  }

  return p->num_cached++;
}

static Page* cache_lookup(Pager* p, uint32_t page_no)
{
  for (int i = 0; i < p->num_cached; ++i)
  {
    if (p->cache[i]->page_no == page_no)
      return p->cache[i];
  }
  return NULL;
}

static PagerStatus cache_load(Pager* p, Page** page, uint32_t page_no)
{
  Page* pg;
  PagerStatus s = create_page(&pg, page_no);
  if (s != PAGER_OK)
    return s;

  if (load(p, pg) != PAGE_SIZE)
  {
    free(pg);
    return PAGER_ERR_IO;
  }

  uint32_t cache_index = find_victim(p);
  if (cache_index == UINT32_MAX)
  {
    free(pg->data);
    free(pg);
    return PAGER_ERR_IO;
  }

  p->cache[cache_index] = pg;
  *page                 = pg;

  return PAGER_OK;
}

PagerStatus p_open(const char* filename, Pager** out)
{
  Pager* p = calloc(1, sizeof(Pager));
  if (p == NULL)
    return PAGER_ERR_NO_MEM;

  char temp_file[256];
  snprintf(temp_file, sizeof(temp_file), "%s.db", filename);

  int fd = open(filename, O_RDWR | O_CREAT, 0644);
  if (fd == -1)
  {
    free(p);
    return PAGER_ERR_IO;
  }
  p->fd = fd;

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    close(fd);
    free(p);
    return PAGER_ERR_IO;
  }

  if (st.st_size % PAGE_SIZE != 0)
  {
    close(fd);
    free(p);
    return PAGER_ERR_CORRUPT;
  }

  if (st.st_size != 0)
    p->num_pages = (st.st_size / PAGE_SIZE);
  else
    p->num_pages = 0;

  *out = p;

  return PAGER_OK;
}

PagerStatus p_close(Pager* p)
{
  if (close(p->fd) == -1)
    return PAGER_ERR_IO;

  for (int i = 0; i < p->num_cached; ++i)
  {
    free(p->cache[i]->data);
    free(p->cache[i]);
  }

  free(p);
  return PAGER_OK;
}

PagerStatus p_alloc_page(Pager* p, uint32_t* page_no)
{
  if (p == NULL)
    return PAGER_ERR_BAD_PAGER;

  Page* pg;
  PagerStatus s = create_page(&pg, p->num_pages);
  if (s != PAGER_OK)
    return s;

  uint32_t cache_index = find_victim(p);
  if (cache_index == UINT32_MAX)
  {
    free(pg->data);
    free(pg);
    return PAGER_ERR_IO;
  }

  p->cache[cache_index] = pg;
  *page_no              = p->num_pages++;

  return PAGER_OK;
}

PagerStatus p_read_page(Pager* p, uint32_t page_no, void** page_data)
{
  if (p == NULL)
    return PAGER_ERR_BAD_PAGER;

  if (page_no >= p->num_pages)
    return PAGER_ERR_INVALID_PAGE;

  Page* pg = cache_lookup(p, page_no);
  if (pg == NULL)
  {
    PagerStatus s = cache_load(p, &pg, page_no);
    if (s != PAGER_OK)
      return s;
  }

  pg->ref    = true;
  *page_data = pg->data;

  return PAGER_OK;
}

PagerStatus p_write_page(Pager* p, uint32_t page_no, void* page_data)
{
  if (p == NULL)
    return PAGER_ERR_BAD_PAGER;

  if (page_no >= p->num_pages)
    return PAGER_ERR_INVALID_PAGE;

  Page* pg = cache_lookup(p, page_no);
  if (pg == NULL)
  {
    PagerStatus s = cache_load(p, &pg, page_no);
    if (s != PAGER_OK)
      return s;
  }

  pg->ref       = true;
  pg->dirty     = true;
  pg->was_dirty = true;
  memcpy(pg->data, page_data, PAGE_SIZE);

  return PAGER_OK;
}

PagerStatus p_commit(Pager* p)
{
  if (p == NULL)
    return PAGER_ERR_BAD_PAGER;

  for (int i = 0; i < p->num_cached; ++i)
  {
    if (p->cache[i]->dirty)
    {
      if (flush(p, p->cache[i]) != PAGE_SIZE)
        return PAGER_ERR_IO;

      p->cache[i]->dirty     = false;
      p->cache[i]->was_dirty = false;
    }
  }

  if (fsync(p->fd) == -1)
    return PAGER_ERR_IO;

  return PAGER_OK;
}