#include "journal.h"
#include "pager.h"
#include "vector.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static int j_read(Journal* j, JournalEntry* e, uint32_t entry_no)
{
  return pread(j->fd, e, JOURNAL_ENTRY_SIZE, entry_no * JOURNAL_ENTRY_SIZE);
}

static int j_write(Journal* j, JournalEntry* e, uint32_t entry_no)
{
  return pwrite(j->fd, e, JOURNAL_ENTRY_SIZE, entry_no * JOURNAL_ENTRY_SIZE);
}

int j_open(Journal** out, const char* journal_filename)
{
  Journal* j = calloc(1, sizeof(Journal));

  Vector* v;
  if (v_open(&v) != VECTOR_OK)
    return -1;

  j->journal = v;
  strcat(j->filename, journal_filename);
  strcat(j->filename, JOURNAL_TAG);

  if ((j->fd = open(journal_filename, O_RDWR)) == -1)
    j->fd = open(journal_filename, O_RDWR | O_CREAT, 0644);

  if (flock(j->fd, LOCK_EX | LOCK_NB) == -1)
  {
    close(j->fd);
    v_close(j->journal);
    free(j);
    return -1;
  }

  if ()

    *out = j;

  return 0;
}