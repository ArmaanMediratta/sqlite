#ifndef JOURNAL_H
#define JOURNAL_H

#include "constants.h"
#include "vector.h"

#define JOURNAL_ENTRY_SIZE (PAGE_SIZE + sizeof(uint32_t))

typedef struct Pager Pager;

typedef enum
{
  JOURNAL_OK,
  JOURNAL_ERR_MEM,
  JOURNAL_ERR_IO,
  JOURNAL_ERR_BAD_JOURNAL,
  JOURNAL_ERR_EXTERNAL,
  JOURNAL_ERR_LOCKED
} JournalStatus;

typedef struct __attribute__((packed)) JournalHeader
{
  char start;
  char end;
  char pad[JOURNAL_ENTRY_SIZE - (2 * sizeof(char))];
} JournalHeader;

typedef struct __attribute__((packed)) JournalEntry
{
  uint32_t page_no;
  char data[PAGE_SIZE];
} JournalEntry;

typedef struct Journal
{
  int fd;
  char filename[256];
  Vector* journal;
} Journal;

static const char START_CHAR         = '@';
static const char END_CHAR           = '?';
static const char* const JOURNAL_TAG = "-journal";

JournalStatus j_open(Journal** out, const char* journal_filename);
JournalStatus j_close(Journal* j);

JournalStatus j_create(Journal* j);
JournalStatus j_add_entry(Journal* j, uint32_t page_no, void* data);
JournalStatus j_commit(Journal* j);
JournalStatus j_rollback(Journal* j, Pager* p);

#endif