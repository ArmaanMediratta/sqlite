#include "pager.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

PagerStatus p_open(const char* filename, Pager** out)
{
  Pager* p = calloc(1, sizeof(Pager));
  if (p == NULL)
  {
    return PAGER_ERR_NOMEM;
  }

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
  {
    p->num_pages = (st.st_size / PAGE_SIZE);
  }
  else
  {
    p->num_pages = 0;
  }

  for (int i = 0; i < CACHE_SIZE; ++i)
  {
    p->cache[i].page_no = UINT32_MAX;
  }
  *out = p;

  return PAGER_OK;
}

PagerStatus p_close(Pager* p)
{
}

PagerStatus p_alloc_page(Pager* p, uint32_t* page_no)
{
}

PagerStatus p_read_page(Pager* p, uint32_t page_no, void** page_data)
{
}

PagerStatus p_write_page(Pager* p, uint32_t page_no, void* page_data)
{
}
