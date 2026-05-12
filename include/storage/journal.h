#ifndef JOURNAL_H
#define JOURNAL_H

#include "pager.h"
#include "vector.h"

typedef enum
{
  JOURNAL_OK,
  JOURNAL_ERR_MEM,
  JOURNAL_ERR_BAD_JOURNAL
} JournalStatus;

typedef struct __attribute__((packed))
{
  uint32_t page_no;
  char data[PAGE_SIZE];
} JournalEntry;

typedef struct
{
  char* filename;
  int fd;
  Vector* journal;
} Journal;

static const char START_CHAR             = '@';
static const char END_CHAR               = '?';
static const char* const JOURNAL_TAG     = "-journal";
static const uint32_t JOURNAL_ENTRY_SIZE = sizeof(JournalEntry);

#endif