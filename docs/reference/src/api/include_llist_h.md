# include/llist.h

## `#define llist_for_each(pos, n, head, member) \ for (pos = list_entry((head)->next, typeof(*pos), member), \ n = list_entry(pos->member.next, typeof(*pos), member);`


list_for_each_entry	-	iterate over list of given type
@pos:	the type * to use as a loop counter.
@head:	the head for your list.
@member:	the name of the list_struct within the struct.


---

