#include <linked_list.h>

#include <stdio.h>
#include <stdlib.h>

Linked_List* linked_list_create()
{
  Linked_List* New = calloc(1, sizeof(Linked_List)); /* zeroed allocation, just what we need */
  if (!New)
    return NULL;

  New->count = 0;

  return New;
}

int linked_list_item_add(Linked_List* _List, Linked_Item** _Item_Ptr, void* _item)
{
  /* Allocate memory in the heap for a new Linked_Item */
  Linked_Item* New_Item = malloc(sizeof(Linked_Item));
  if (New_Item == NULL)
    return -1;

  New_Item->prev = NULL;
  New_Item->next = NULL;
  New_Item->item = _item;

  if (_List->tail == NULL) /* If no Item added to List yet */
  {
    _List->head = New_Item;
    _List->tail = New_Item;
  } else {
    New_Item->prev = _List->tail;
    _List->tail->next = New_Item;
    _List->tail = New_Item;
  }

  /* If caller wants to specify the pointer */
  if (_Item_Ptr != NULL)
    *_Item_Ptr = New_Item;

  _List->count++;

  return 0;
}

void linked_list_item_remove(Linked_List* _List, Linked_Item* _Item)
{
  /* Remove Item from List */
  if (_Item->next == NULL && _Item->prev == NULL) /* I'm alone :( */
  {
    _List->head = NULL;
    _List->tail = NULL;
  } else if (_Item->prev == NULL) /* I'm first :) */
  {
    _List->head = _Item->next;
    _Item->next->prev = NULL;
  } else if (_Item->next == NULL) /* I'm last :/ */
  {
    _List->tail = _Item->prev;
    _Item->prev->next = NULL;
  } else /* I'm in the middle :| */
  {
    _Item->prev->next = _Item->next;
    _Item->next->prev = _Item->prev;
  }

  /* Free the Item-related memory and null it */
  free(_Item);
  _Item = NULL;

  _List->count--;
}

void linked_list_items_dispose(Linked_List* _List)
{
  if (_List == NULL) {
    return;
  }

  while (_List->head != NULL) {
    linked_list_item_remove(_List, _List->head);
  }
}

void linked_list_destroy(Linked_List** _List_Ptr)
{
  if (_List_Ptr == NULL || *_List_Ptr == NULL) {
    return;
  }

  linked_list_items_dispose(*_List_Ptr);

  free(*_List_Ptr);
  *_List_Ptr = NULL;
}
