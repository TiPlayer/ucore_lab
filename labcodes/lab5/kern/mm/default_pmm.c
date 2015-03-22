#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>


free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

// LAB2 EXERCISE 1: 2012011295
static void default_init(void) {
  list_init(&free_list);
  nr_free = 0;
}

static void default_init_memmap(struct Page *base, size_t n) {
  assert(n > 0);
  struct Page *p = base;
  for (; p != base + n; p ++) {
    assert(PageReserved(p));
    p->flags = 0;
    SetPageProperty(p);
    p->property = 0;
    set_page_ref(p, 0);
    list_add_before(&free_list, &(p->page_link));
  }
  base->property = n;
  nr_free += n;
}

static struct Page* default_alloc_pages(size_t n) {
  assert(n > 0);
  if (n > nr_free) {
    return NULL;
  }
  struct Page *page = NULL;
  list_entry_t *le = list_next(&free_list);
  for (; le != &free_list; le = list_next(le)) {
    struct Page *p = le2page(le, page_link);
    if (p->property >= n) {
      page = p;
      break;
    }
  }
  if (page != NULL) {
    int i;
    list_entry_t* page_link_to_be_alloced = &page->page_link;
    list_entry_t* link_next = list_next(page_link_to_be_alloced);
    for (i = 0; i < n; i++) {
      link_next = list_next(page_link_to_be_alloced);
      struct Page * page_to_be_alloced = le2page(page_link_to_be_alloced, page_link);
      SetPageReserved(page_to_be_alloced);
      ClearPageProperty(page_to_be_alloced);
      list_del(page_link_to_be_alloced);
      page_link_to_be_alloced = link_next;
    }
    if (page->property > n) {
      struct Page *p = page + n;
      p->property = page->property - n;
    }
    nr_free -= n;
  }
  return page;
}

static void default_free_pages(struct Page *base, size_t n) {
  assert(n > 0);
  assert(PageReserved(base));
  struct Page *p = base;
  list_entry_t *list_after_base = list_next(&free_list);
  for (; list_after_base != &free_list; list_after_base = list_next(list_after_base)) {
    p = le2page(list_after_base, page_link);
    if (p > base) break;
  }
  for (p = base; p < base + n; p++) {
    list_add_before(list_after_base, &p->page_link);
  }
  for (p = base; p != base + n; p ++) {
    assert(PageReserved(p) && !PageProperty(p));
    p->flags = 0;
    p->ref = 0;
    p->property = 0;
    SetPageProperty(p);
    ClearPageReserved(p);
    set_page_ref(p, 0);
  }
  base->property = n;
  if (list_after_base != &free_list) {
    struct Page* page_after_base = le2page(list_after_base, page_link);
    if (page_after_base == base + n) {
      base->property += page_after_base->property;
      page_after_base->property = 0;
    }
  }
  list_entry_t* list_before_base = list_prev(&base->page_link);
  struct Page* page_before = le2page(list_before_base, page_link);
  if (page_before == base - 1) {
    for (; list_before_base != &free_list; list_before_base = list_prev(list_before_base)) {
      struct Page* page_before = le2page(list_before_base, page_link);
      if (page_before->property > 0) {
        page_before->property += base->property;
        base->property = 0;
        break;
      }
    }
  }
  nr_free += n;
}


static size_t
default_nr_free_pages(void) {
  return nr_free;
}

static void
basic_check(void) {
  struct Page *p0, *p1, *p2;
  p0 = p1 = p2 = NULL;
  assert((p0 = alloc_page()) != NULL);
  assert((p1 = alloc_page()) != NULL);
  assert((p2 = alloc_page()) != NULL);
  
  assert(p0 != p1 && p0 != p2 && p1 != p2);
  assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);
  
  assert(page2pa(p0) < npage * PGSIZE);
  assert(page2pa(p1) < npage * PGSIZE);
  assert(page2pa(p2) < npage * PGSIZE);
  
  list_entry_t free_list_store = free_list;
  list_init(&free_list);
  assert(list_empty(&free_list));
  
  unsigned int nr_free_store = nr_free;
  nr_free = 0;
  
  assert(alloc_page() == NULL);
  
  free_page(p0);
  free_page(p1);
  free_page(p2);
  assert(nr_free == 3);
  
  assert((p0 = alloc_page()) != NULL);
  assert((p1 = alloc_page()) != NULL);
  assert((p2 = alloc_page()) != NULL);
  
  assert(alloc_page() == NULL);
  
  free_page(p0);
  assert(!list_empty(&free_list));
  
  struct Page *p;
  assert((p = alloc_page()) == p0);
  assert(alloc_page() == NULL);
  
  assert(nr_free == 0);
  free_list = free_list_store;
  nr_free = nr_free_store;
  
  free_page(p);
  free_page(p1);
  free_page(p2);
}

static void
default_check(void) {
  int count = 0, total = 0;
  list_entry_t *le = &free_list;
  while ((le = list_next(le)) != &free_list) {
    struct Page *p = le2page(le, page_link);
    assert(PageProperty(p));
    count ++, total += p->property;
  }
  assert(total == nr_free_pages());
  
  basic_check();
  
  struct Page *p0 = alloc_pages(5), *p1, *p2;
  assert(p0 != NULL);
  assert(!PageProperty(p0));
  
  list_entry_t free_list_store = free_list;
  list_init(&free_list);
  assert(list_empty(&free_list));
  assert(alloc_page() == NULL);
  
  unsigned int nr_free_store = nr_free;
  nr_free = 0;
  
  free_pages(p0 + 2, 3);
  assert(alloc_pages(4) == NULL);
  assert(PageProperty(p0 + 2) && p0[2].property == 3);
  assert((p1 = alloc_pages(3)) != NULL);
  assert(alloc_page() == NULL);
  assert(p0 + 2 == p1);
  
  p2 = p0 + 1;
  free_page(p0);
  free_pages(p1, 3);
  assert(PageProperty(p0) && p0->property == 1);
  assert(PageProperty(p1) && p1->property == 3);
  
  assert((p0 = alloc_page()) == p2 - 1);
  free_page(p0);
  assert((p0 = alloc_pages(2)) == p2 + 1);
  
  free_pages(p0, 2);
  free_page(p2);
  
  assert((p0 = alloc_pages(5)) != NULL);
  assert(alloc_page() == NULL);
  
  assert(nr_free == 0);
  nr_free = nr_free_store;
  
  free_list = free_list_store;
  free_pages(p0, 5);
  
  le = &free_list;
  while ((le = list_next(le)) != &free_list) {
    struct Page *p = le2page(le, page_link);
    count --, total -= p->property;
  }
  assert(count == 0);
  assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
  .name = "default_pmm_manager",
  .init = default_init,
  .init_memmap = default_init_memmap,
  .alloc_pages = default_alloc_pages,
  .free_pages = default_free_pages,
  .nr_free_pages = default_nr_free_pages,
  .check = default_check,
};

