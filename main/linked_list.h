#ifndef __LINKED_LIST_H__
#define __LINKED_LIST_H__

typedef struct Linked_Item
{
  struct Linked_Item* prev;
  struct Linked_Item* next;

  void* item; /* Item could be anything */
  
} Linked_Item;

typedef struct
{
  Linked_Item* head;
  Linked_Item* tail;

  unsigned int count;

} Linked_List;

#define linked_list_foreach(list, node) \
  for (Linked_Item* node = (list)->head; node != NULL; node = node->next)

/** Adds new Linked_List using calloc for persistent use
 * needs to be destroyed seperately with linked_list_destroy */
Linked_List* linked_list_create();

/* Adds a new item to Linked_list 
 * _Linked_Item_Ptr can be passed as NULL */
int linked_list_item_add(Linked_List* _Linked_List, Linked_Item** _Linked_Item_Ptr, void* _item);

/** Removes specific item from list */
void linked_list_item_remove(Linked_List* _Linked_List, Linked_Item* _Linked_Item);

/** Disposes of all items in the list */
void linked_list_items_dispose(Linked_List* _Linked_List);

/** Destroys the calloced Linked_List (also runs linked_list_items_dispose) */
void linked_list_destroy(Linked_List** _List_Ptr);

#endif
