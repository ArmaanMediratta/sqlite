#include "journal.h"
#include "constants.h"
#include "pager.h"
#include "vector.h"
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

static int j_read(int fd, void* buf, int entry_no, int size)
{
  return pread(fd, buf, size, entry_no * size);
}

static int j_write(int fd, void* buf, int entry_no, int size)
{
  return pwrite(fd, buf, size, entry_no * size);
}

static int j_size(Journal* j)
{
  struct stat st;
  if (fstat(j->fd, &st) == -1)
    return -1;

  if (st.st_size % JOURNAL_ENTRY_SIZE != 0)
    return -1;

  return st.st_size / JOURNAL_ENTRY_SIZE;
}

static bool j_check_header(Journal* j)
{
  JournalHeader jh;
  if (j_read(j->fd, &jh, 0, JOURNAL_ENTRY_SIZE) == -1)
    return false;

  return jh.start == START_CHAR && jh.end == END_CHAR;
}

JournalStatus j_open(Journal** out, const char* journal_filename)
{
  Journal* j = calloc(1, sizeof(Journal));
  if (j == NULL)
    return JOURNAL_ERR_MEM;

  Vector* v;
  if (v_open(&v) != VECTOR_OK)
  {
    free(j);
    return JOURNAL_ERR_EXTERNAL;
  }

  j->journal = v;
  snprintf(j->filename, sizeof(j->filename), "%s%s", journal_filename,
           JOURNAL_TAG);

  if ((j->fd = open(j->filename, O_RDWR)) == -1)
    j->fd = open(j->filename, O_RDWR | O_CREAT, 0644);

  if (flock(j->fd, LOCK_EX | LOCK_NB) == -1)
  {
    close(j->fd);
    v_close(j->journal);
    free(j);
    return JOURNAL_ERR_LOCKED;
  }

  *out = j;

  return 0;
}

JournalStatus j_close(Journal* j)
{
  flock(j->fd, LOCK_UN);
  close(j->fd);
  v_close(j->journal);
  remove(j->filename);
  free(j);
  return JOURNAL_OK;
}

JournalStatus j_create(Journal* j)
{
  if (j == NULL)
    return JOURNAL_ERR_BAD_JOURNAL;

  if (ftruncate(j->fd, 0) == -1)
    return JOURNAL_ERR_IO;

  JournalHeader jh;
  jh.start = START_CHAR;

  if (j_write(j->fd, (void*)&jh, 0, JOURNAL_ENTRY_SIZE) == -1)
    return JOURNAL_ERR_IO;

  return JOURNAL_OK;
}

JournalStatus j_add_entry(Journal* j, uint32_t page_no, void* data)
{
  if (j == NULL)
    return JOURNAL_ERR_BAD_JOURNAL;

  Entry* e = calloc(1, sizeof(Entry));
  if (e == NULL)
    return JOURNAL_ERR_BAD_JOURNAL;

  JournalEntry* je = calloc(1, sizeof(JournalEntry));
  if (je == NULL)
  {
    free(e);
    return JOURNAL_ERR_BAD_JOURNAL;
  }

  je->page_no = page_no;
  memcpy(je->data, data, PAGE_SIZE);
  e->data = je;

  if (v_add(j->journal, e) != VECTOR_OK)
  {
    free(je);
    free(e);
    return JOURNAL_ERR_EXTERNAL;
  }

  return JOURNAL_OK;
}

JournalStatus j_commit(Journal* j)
{
  if (j == NULL)
    return JOURNAL_ERR_BAD_JOURNAL;

  for (uint32_t i = 0; i < j->journal->size; ++i)
  {
    if (j_write(j->fd, j->journal->entries[i]->data, i + 1,
                JOURNAL_ENTRY_SIZE) == -1)
      return JOURNAL_ERR_IO;
  }

  if (fsync(j->fd) == -1)
    return JOURNAL_ERR_IO;

  JournalHeader jh;
  jh.start = START_CHAR;
  jh.end   = END_CHAR;
  if (j_write(j->fd, &jh, 0, JOURNAL_ENTRY_SIZE) == -1 || fsync(j->fd) == -1)
    return JOURNAL_ERR_IO;

  return JOURNAL_OK;
}

JournalStatus j_rollback(Journal* j, Pager* p)
{
  if (j == NULL)
    return JOURNAL_ERR_BAD_JOURNAL;

  if (j_check_header(j))
  {
    uint32_t journal_size;

    if ((journal_size = j_size(j)) > 1)
    {
      for (uint32_t i = 1; i < journal_size; i++)
      {
        JournalEntry je;

        if (j_read(j->fd, &je, i, JOURNAL_ENTRY_SIZE) == -1)
          return JOURNAL_ERR_IO;

        if (j_write(p->fd, je.data, je.page_no, PAGE_SIZE) == -1)
          return JOURNAL_ERR_IO;
      }
    }

    if (fsync(j->fd) == -1)
      return JOURNAL_ERR_IO;
  }

  return JOURNAL_OK;
}