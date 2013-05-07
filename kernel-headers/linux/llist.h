#ifndef __LLIST_H__
#define __LLIST_H__

struct list_head {
    struct list_head *prev, *next;
};

/* Get an entry from the list 
 *  ptr - the address of this list_head element in "type" 
 *  type - the data type that contains "member"
 *  member - the list_head element in "type" 
 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0L)->member)))

/* Get each entry from a list
 *  pos - A structure pointer has a "member" element
 *  head - list head
 *  member - the list_head element in "pos"
 */
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                    \
         pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)          \
        for (pos = list_entry((head)->next, typeof(*pos), member),  \
        n = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head);                    \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_empty(entry) ((entry)->next == (entry))

static inline void list_init(struct list_head *entry) {
    entry->prev = entry->next = entry;
}

static inline void list_add(struct list_head *entry, struct list_head *head) {
    entry->next = head->next;
    entry->prev = head;

    head->next->prev = entry;
    head->next = entry;
}

static inline void list_add_tail(struct list_head *entry,
        struct list_head *head) {
    entry->next = head;
    entry->prev = head->prev;

    head->prev->next = entry;
    head->prev = entry;
}

static inline void list_del(struct list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

#endif
