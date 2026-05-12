#include "constants.h"
#include "journal.h"
#include "pager.h"
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int failures = 0;

#define TEST_DB      "test.db"
#define TEST_JOURNAL "test.db-journal"

#define RUN_TEST(fn)                                                           \
  do                                                                           \
  {                                                                            \
    pid_t pid = fork();                                                        \
    if (pid == 0)                                                              \
    {                                                                          \
      fn();                                                                    \
      exit(0);                                                                 \
    }                                                                          \
    else                                                                       \
    {                                                                          \
      int status;                                                              \
      waitpid(pid, &status, 0);                                                \
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0)                       \
      {                                                                        \
        printf("%s PASSED\n", #fn);                                            \
      }                                                                        \
      else                                                                     \
      {                                                                        \
        printf("%s FAILED\n", #fn);                                            \
        failures = 1;                                                          \
      }                                                                        \
    }                                                                          \
  } while (0)

void cleanup(Journal* j, Pager* p)
{
  if (j)
    j_close(j);
  if (p)
    p_close(p);

  remove(TEST_DB);
}

void populate_db()
{
  Pager* p;
  p_open(TEST_DB, &p);

  for (uint32_t i = 0; i < 5; ++i)
  {
    p_alloc_page(p, &i);
    char data[PAGE_SIZE];
    memset(data, 1, PAGE_SIZE);
    p_write_page(p, i, data);
  }
  p_commit(p);
}

void add_entries(Journal* j, int count)
{
  for (int i = 0; i < count; ++i)
  {
    char data[PAGE_SIZE] = {0};
    j_add_entry(j, i, data);
  }
}

void create_good_existing_journal(int count)
{
  Journal* j;
  j_open(&j, TEST_DB);
  add_entries(j, count);
  j_commit(j);
  flock(j->fd, LOCK_UN);
}

void create_corrupt_existing_journal()
{
  Journal* j;
  j_open(&j, TEST_DB);
  add_entries(j, 5);
  flock(j->fd, LOCK_UN);
}

uint32_t count_database_entries()
{
  struct stat st;
  stat(TEST_DB, &st);
  return st.st_size / PAGE_SIZE;
}

uint32_t count_journal_entries()
{
  struct stat st;
  stat(TEST_JOURNAL, &st);
  return st.st_size / JOURNAL_ENTRY_SIZE;
}

bool valid_journal(Journal* j)
{
  JournalHeader jh;
  pread(j->fd, &jh, JOURNAL_ENTRY_SIZE, 0 * JOURNAL_ENTRY_SIZE);
  return jh.start == START_CHAR && jh.end == END_CHAR;
}

void test_open_close_no_journal()
{
  Journal* j;

  assert(j_open(&j, TEST_DB) == JOURNAL_OK);
  assert(j != NULL);

  cleanup(j, NULL);
}

void test_open_close_existing_journal()
{
  create_good_existing_journal(1);
  Journal* j;

  assert(j_open(&j, TEST_DB) == JOURNAL_OK);
  assert(j != NULL);

  cleanup(j, NULL);
}

void test_complete_journal_lifecycle()
{
  Journal* j;
  j_open(&j, TEST_DB);
  add_entries(j, 5);
  j_commit(j);

  assert(count_journal_entries() == 5 + 1); // +1 for header
  assert(valid_journal(j));

  cleanup(j, NULL);
}

void test_incomplete_journal_lifecycle()
{
  Journal* j;
  j_open(&j, TEST_DB);
  add_entries(j, 5);

  assert(!valid_journal(j));

  cleanup(j, NULL);
}

void test_valid_journal_rollback()
{
  populate_db();

  Pager* p;
  p_open(TEST_DB, &p);

  create_good_existing_journal(5);

  Journal* j;
  j_open(&j, TEST_DB);
  j_rollback(j, p);

  assert(count_database_entries() == 5);

  for (uint32_t i = 0; i < 5; ++i)
  {
    void* buf;
    p_read_page(p, i, &buf);
    char* cbuf = (char*)buf;

    for (uint32_t k = 0; k < PAGE_SIZE; ++k)
      assert(cbuf[k] == 0);
  }

  cleanup(j, p);
}

void test_invalid_journal_rollback()
{
  populate_db();

  Pager* p;
  p_open(TEST_DB, &p);

  create_corrupt_existing_journal();

  Journal* j;
  j_open(&j, TEST_DB);
  j_rollback(j, p);

  assert(count_database_entries() == 5);

  for (uint32_t i = 0; i < 5; ++i)
  {
    void* buf;
    p_read_page(p, i, &buf);
    char* cbuf = (char*)buf;

    for (uint32_t k = 0; k < PAGE_SIZE; ++k)
      assert(cbuf[k] == 1);
  }

  cleanup(j, p);
}

int main()
{
  printf("Running tests...\n\n");

  printf("----------JOURNAL OPEN----------\n");
  RUN_TEST(test_open_close_no_journal);
  RUN_TEST(test_open_close_existing_journal);
  printf("\n");

  printf("----------JOURNAL LIFECYCLE----------\n");
  RUN_TEST(test_complete_journal_lifecycle);
  RUN_TEST(test_incomplete_journal_lifecycle);
  printf("\n");

  printf("----------JOURNAL ROLLBACK----------\n");
  RUN_TEST(test_valid_journal_rollback);
  RUN_TEST(test_invalid_journal_rollback);
  printf("\n");

  printf("All tests ran!\n");

  return failures;
}