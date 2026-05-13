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

#define TEST_DB_NAME "test"
#define TEST_DB      "test.db"

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

void cleanup(Pager* p)
{
  if (p)
    p_close(p);
  remove(TEST_DB);
  remove(TEST_DB_NAME);
}

void allocate_pages(Pager* p, uint32_t num_pages)
{
  for (uint32_t i = 0; i < num_pages; ++i)
  {
    uint32_t pg_no;
    p_alloc_page(p, &pg_no);
  }
}

void write_pages(Pager* p, uint32_t num_pages)
{
  char TEST_DATA[PAGE_SIZE];
  memset(TEST_DATA, 'A', PAGE_SIZE);

  for (uint32_t i = 0; i < num_pages; ++i)
    if (i < p->num_pages)
      p_write_page(p, i, TEST_DATA);
}

uint32_t count_pages_on_disk()
{
  struct stat st;
  stat(TEST_DB, &st);
  return st.st_size / PAGE_SIZE;
}

void test_open_close_empty_db()
{
  Pager* p;

  assert(p_open(&p, TEST_DB_NAME) == PAGER_OK);
  assert(p != NULL);
  assert(p->num_pages == 0);
  assert(p->j != NULL);

  cleanup(p);
}

void test_open_close_non_empty_db()
{
  Pager* p;
  p_open(&p, TEST_DB_NAME);
  allocate_pages(p, 64);
  write_pages(p, 64);
  p_commit(p);
  p_close(p);

  assert(p_open(&p, TEST_DB_NAME) == PAGER_OK);
  assert(p != NULL);
  assert(p->num_pages == 64);
  assert(p->j != NULL);

  cleanup(p);
}

void test_corrupt_db()
{
  int fd = open(TEST_DB, O_RDWR | O_CREAT, 0644);
  write(fd, "a", 1);

  Pager* p;
  assert(p_open(&p, TEST_DB_NAME) == PAGER_ERR_CORRUPT);

  cleanup(NULL);
  close(fd);
}

void test_allocate_page_empty_db()
{
  Pager* p;
  uint32_t page_no;
  p_open(&p, TEST_DB_NAME);

  assert(p_alloc_page(p, &page_no) == PAGER_OK);

  assert(p->num_pages == 1);

  assert(p->cache[0] != NULL);
  assert(!p->cache[0]->dirty && !p->cache[0]->was_dirty && !p->cache[0]->ref);
  assert(p->cache[0]->page_no == 0);
  assert(p->cache[0]->data != NULL);

  cleanup(p);
}

void test_allocate_page_partial_cache()
{
  Pager* p;
  uint32_t num_alloc = 32;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, num_alloc);

  assert(p->num_pages == num_alloc);

  for (uint32_t i = 0; i < num_alloc; ++i)
  {
    assert(!p->cache[i]->dirty && !p->cache[i]->was_dirty && !p->cache[i]->ref);
    assert(p->cache[i]->page_no == i);
    assert(p->cache[i]->data != NULL);
  }

  cleanup(p);
}

void test_allocate_page_full_cache()
{
  Pager* p;
  uint32_t num_alloc = 64;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, num_alloc);

  uint32_t page_no;
  assert(p_alloc_page(p, &page_no) == PAGER_OK);

  assert(page_no == 64);
  assert(p->num_pages == num_alloc + 1);

  assert(p->cache[0]->page_no == page_no);

  for (uint32_t i = 0; i < num_alloc; ++i)
  {
    assert(!p->cache[i]->dirty && !p->cache[i]->was_dirty && !p->cache[i]->ref);
    assert(p->cache[i]->data != NULL);
  }

  cleanup(p);
}

void test_read_invalid_page()
{
  Pager* p;
  void* data;
  p_open(&p, TEST_DB_NAME);

  assert(p_read_page(p, 100, &data) == PAGER_ERR_INVALID_PAGE);

  cleanup(p);
}

void test_read_empty_page()
{
  Pager* p;
  void* data;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 1);

  assert(p_read_page(p, 0, &data) == PAGER_OK);

  assert(p->cache[0]->data != NULL);
  assert(p->cache[0]->ref);

  char* clean_data = (char*)data;
  for (uint32_t i = 0; i < PAGE_SIZE; ++i)
    assert(clean_data[i] == 0);

  cleanup(p);
}

void test_read_non_empty_page()
{
  Pager* p;
  void* data;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 1);
  write_pages(p, 1);

  assert(p_read_page(p, 0, &data) == PAGER_OK);

  assert(p->cache[0]->data != NULL);
  assert(p->cache[0]->ref);

  char* clean_data = (char*)data;
  for (uint32_t i = 0; i < PAGE_SIZE; ++i)
    assert(clean_data[i] == 'A');

  cleanup(p);
}

void test_read_from_disk()
{
  Pager* p;
  void* data;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 1);
  write_pages(p, 1);
  p_commit(p);

  assert(p_read_page(p, 0, &data) == PAGER_OK);

  assert(p->cache[0]->data != NULL);
  assert(p->cache[0]->ref);

  char* clean_data = (char*)data;
  for (uint32_t i = 0; i < PAGE_SIZE; ++i)
    assert(clean_data[i] == 'A');

  cleanup(p);
}

void test_write_page()
{
  {
    Pager* p;
    void* data[4096];
    p_open(&p, TEST_DB_NAME);
    allocate_pages(p, 1);

    assert(p_write_page(p, 0, data) == PAGER_OK);

    assert(p->cache[0]->data != NULL);
    assert(p->cache[0]->ref && p->cache[0]->dirty && p->cache[0]->was_dirty);

    char* clean_data = (char*)p->cache[0]->data;
    for (uint32_t i = 0; i < PAGE_SIZE; ++i)
      assert(clean_data[i] == 0);

    cleanup(p);
  }
}

void test_evict_clean_page()
{
  Pager* p;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 64);

  uint32_t page_no;
  assert(p_alloc_page(p, &page_no) == PAGER_OK);

  assert(page_no == 64);
  assert(p->num_pages == 65);

  assert(p_commit(p) == PAGER_OK);
  assert(count_pages_on_disk() == 0);

  cleanup(p);
}

void test_evict_prefers_clean_over_dirty()
{
  Pager* p;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 64);

  char data[PAGE_SIZE];
  memset(data, 'A', PAGE_SIZE);
  assert(p_write_page(p, 0, data) == PAGER_OK);

  uint32_t page_no;
  assert(p_alloc_page(p, &page_no) == PAGER_OK);

  for (uint32_t i = 0; i < CACHE_SIZE; ++i)
    if (p->cache[i] && p->cache[i]->page_no == 0)
    {
      assert(p->cache[i]->dirty);
      break;
    }

  cleanup(p);
}

void test_evict_dirty_page_flushed_to_disk()
{
  Pager* p;
  p_open(&p, TEST_DB_NAME);

  allocate_pages(p, 64);
  write_pages(p, 64);

  assert(p_commit(p) == PAGER_OK);
  assert(count_pages_on_disk() == 64);

  cleanup(p);
}

int main()
{
  printf("Running tests...\n\n");

  printf("----------PAGER OPEN----------\n");
  RUN_TEST(test_open_close_empty_db);
  RUN_TEST(test_open_close_non_empty_db);
  RUN_TEST(test_corrupt_db);
  printf("\n");

  printf("----------PAGER ALLOCATE----------\n");
  RUN_TEST(test_allocate_page_empty_db);
  RUN_TEST(test_allocate_page_partial_cache);
  RUN_TEST(test_allocate_page_full_cache);
  printf("\n");

  printf("----------PAGER READ----------\n");
  RUN_TEST(test_read_invalid_page);
  RUN_TEST(test_read_empty_page);
  RUN_TEST(test_read_non_empty_page);
  RUN_TEST(test_read_from_disk);
  printf("\n");

  printf("----------PAGER WRITE----------\n");
  RUN_TEST(test_write_page);
  printf("\n");

  printf("----------PAGER EVICTION----------\n");
  RUN_TEST(test_evict_clean_page);
  RUN_TEST(test_evict_prefers_clean_over_dirty);
  RUN_TEST(test_evict_dirty_page_flushed_to_disk);
  printf("\n");

  printf("All tests ran!\n");

  return failures;
}