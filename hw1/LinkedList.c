/*
 * Copyright 2012 Steven Gribble
 *
 *  This file is part of the UW CSE 333 project sequence (333proj12).
 *
 *  333proj12 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published
 *  by the Free Software Foundation, either version 3 of the License,
 *  or (at your option) any later version.
 *
 *  333proj12 is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with 333proj12.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "Assert333.h"
#include "LinkedList.h"
#include "LinkedList_priv.h"

LinkedList AllocateLinkedList(void) {
  // allocate the linked list record
  LinkedList ll = (LinkedList) malloc(sizeof(LinkedListHead));
  if (ll == NULL) {
    // out of memory
    return (LinkedList) NULL;
  }

  // initialize the newly allocated record structure
	ll->num_elements = 0U;
	ll->head = ll->tail = NULL;

  // return our newly minted linked list
  return ll;
}

void FreeLinkedList(LinkedList list,
                    LLPayloadFreeFnPtr payload_free_function) {
  // defensive programming: check arguments for sanity.
  Assert333(list != NULL);
  Assert333(payload_free_function != NULL);

  // sweep through the list and free all of the nodes' payloads as
  // well as the nodes themselves
  while (list->head != NULL) {
		payload_free_function(list->head->payload);
		LinkedListNodePtr next = list->head->next;
		free(list->head);
		list->head = next;
  }

  // free the list record
  free(list);
	list = NULL;
}

uint64_t NumElementsInLinkedList(LinkedList list) {
  // defensive programming: check argument for safety.
  Assert333(list != NULL);
  return list->num_elements;
}

bool PushLinkedList(LinkedList list, void *payload) {
  // defensive programming: check argument for safety. The user-supplied
  // argument can be anything, of course, so we need to make sure it's
  // reasonable (e.g., not NULL).
  Assert333(list != NULL);

  // allocate space for the new node.
  LinkedListNodePtr ln =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (ln == NULL) {
    // out of memory
    return false;
  }

  // set the payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // degenerate case; list is currently empty
    Assert333(list->head == NULL);  // debugging aid
    Assert333(list->tail == NULL);  // debugging aid
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1U;
    return true;
  }

  // typical case; list has >=1 elements
	list->num_elements++;
	list->head->prev = ln;
	ln->prev = NULL;
	ln->next = list->head;
	list->head = ln;

  // return success
  return true;
}

bool PopLinkedList(LinkedList list, void **payload_ptr) {
  // defensive programming.
  Assert333(payload_ptr != NULL);
  Assert333(list != NULL);

	if (list->num_elements == 0) {
		// called pop on an empty list; return failure
		return false;
	}
	
	// copy the value of the payload to return
	*payload_ptr = list->head->payload;

	if (list->num_elements == 1) {
		// edge case; a list with single element
		free(list->head);
		list->head = list->tail = NULL;
	} else {
		// typical case; list has >= 2 elements
		LinkedListNodePtr oldHead = list->head;
		list->head = oldHead->next;
		list->head->prev = NULL;
		free(oldHead);
		oldHead = NULL;
	}
	list->num_elements--;

	// return success
  return true;
}

bool AppendLinkedList(LinkedList list, void *payload) {
  // defensive programming: check argument for safety.
  Assert333(list != NULL);

  // allocate space for the new node.
  LinkedListNodePtr ln =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (ln == NULL) {
    // out of memory; return failure
    return false;
  }

  // set the payload
  ln->payload = payload;

  if (list->num_elements == 0) {
    // degenerate case; list is currently empty
    Assert333(list->head == NULL);  // debugging aid
    Assert333(list->tail == NULL);  // debugging aid
    ln->next = ln->prev = NULL;
    list->head = list->tail = ln;
    list->num_elements = 1U;

		// return success
    return true;
  }

  // typical case; list has >=1 elements
	list->num_elements++;
	list->tail->next = ln;
	ln->next = NULL;
	ln->prev = list->tail;
	list->tail = ln;

  // return success
  return true;
}

bool SliceLinkedList(LinkedList list, void **payload_ptr) {
  // defensive programming.
  Assert333(payload_ptr != NULL);
  Assert333(list != NULL);

	if (list->num_elements == 0) {
		// called slice on an empty list; return failure
		return false;
	}

	// copy the value of the payload to return
	*payload_ptr = list->tail->payload;

	if (list->num_elements == 1) {
		// edge case; a list with single element
		free(list->tail);
		list->head = list->tail = NULL;
	} else {
		// typical case; list has >= 2 elements
		LinkedListNodePtr oldTail = list->tail;
		list->tail = oldTail->prev;
		list->tail->next = NULL;
		free(oldTail);
		oldTail = NULL;
		
	}
	list->num_elements--;

	// return success
  return true;
}

void SortLinkedList(LinkedList list, unsigned int ascending,
                    LLPayloadComparatorFnPtr comparator_function) {
  Assert333(list != NULL);  // defensive programming
  if (list->num_elements < 2) {
    // no sorting needed
    return;
  }

  // we'll implement bubblesort! nice and easy, and nice and slow :)
  int swapped;
  do {
    LinkedListNodePtr curnode;

    swapped = 0;
    curnode = list->head;
    while (curnode->next != NULL) {
      int compare_result = comparator_function(curnode->payload,
                                               curnode->next->payload);
      if (ascending) {
        compare_result *= -1;
      }
      if (compare_result < 0) {
        // bubble-swap payloads
        void *tmp;
        tmp = curnode->payload;
        curnode->payload = curnode->next->payload;
        curnode->next->payload = tmp;
        swapped = 1;
      }
      curnode = curnode->next;
    }
  } while (swapped);
}

LLIter LLMakeIterator(LinkedList list, int pos) {
  // defensive programming
  Assert333(list != NULL);
  Assert333((pos == 0) || (pos == 1));

  // if the list is empty, return failure.
  if (NumElementsInLinkedList(list) == 0U)
    return NULL;

  // OK, let's manufacture an iterator.
  LLIter li = (LLIter) malloc(sizeof(LLIterSt));
  if (li == NULL) {
    // out of memory!
    return NULL;
  }

  // set up the iterator.
  li->list = list;
  if (pos == 0) {
    li->node = list->head;
  } else {
    li->node = list->tail;
  }

  // return the new iterator
  return li;
}

void LLIteratorFree(LLIter iter) {
  // defensive programming
  Assert333(iter != NULL);
  free(iter);
}

bool LLIteratorHasNext(LLIter iter) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // Is there another node beyond the iterator?
  if (iter->node->next == NULL)
    return false;  // no

  return true;  // yes
}

bool LLIteratorNext(LLIter iter) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // if there is another node beyond the iterator, advance to it,
  // and return true.
	if (iter->list->tail != iter->node) {
		// iterator not on the tail; iterate to the next node
		iter->node = iter->node->next;
		return true;
	}

  // Nope, there isn't another node, so return failure.
  return false;
}

bool LLIteratorHasPrev(LLIter iter) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // Is there another node beyond the iterator?
  if (iter->node->prev == NULL)
    return false;  // no

  return true;  // yes
}

bool LLIteratorPrev(LLIter iter) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // if there is another node beyond the iterator, advance to it,
  // and return true.
	if (iter->list->head != iter->node) {
		// iterator not on the head; iterate to the previous node
		iter->node = iter->node->prev;
		return true;
	}	

  // nope, so return failure.
  return false;
}

void LLIteratorGetPayload(LLIter iter, void **payload) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // set the return parameter.
  *payload = iter->node->payload;
}

bool LLIteratorDelete(LLIter iter,
                      LLPayloadFreeFnPtr payload_free_function) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

	// free the payload of the current node
	payload_free_function(iter->node->payload);

	if (LLIteratorHasNext(iter) && LLIteratorHasPrev(iter)) {
		// fully general case: iter points in the middle of a list
		LinkedListNodePtr successor = iter->node->next;
		successor->prev = iter->node->prev;
		iter->node->prev->next = successor;
		free(iter->node);
		iter->node = successor;
		iter->list->num_elements--;
	} else if (LLIteratorHasNext(iter)) {
		// degenerate case: iter points at head
		iter->node = iter->node->next;
		iter->list->head = iter->node;
		free(iter->node->prev);
		iter->node->prev = NULL;
		iter->list->num_elements--;
	} else if (LLIteratorHasPrev(iter)) {
		// degenerate case: iter points at tail
		iter->node = iter->node->prev;
		iter->list->tail = iter->node;
		free(iter->node->next);
		iter->node->next = NULL;
		iter->list->num_elements--;
	} else {
		// degenerate case: the list becomes empty after deleting
		iter->list->head = iter->list->tail = NULL;
		iter->list->num_elements--;
		free(iter->node);
		iter->node = NULL;

		// return failure
		return false;
	}

	// return success
	return true;
}

bool LLIteratorInsertBefore(LLIter iter, void *payload) {
  // defensive programming
  Assert333(iter != NULL);
  Assert333(iter->list != NULL);
  Assert333(iter->node != NULL);

  // If the cursor is pointing at the head, use our
  // PushLinkedList function.
  if (iter->node == iter->list->head) {
    return PushLinkedList(iter->list, payload);
  }

  // General case: we have to do some splicing.
  LinkedListNodePtr newnode =
    (LinkedListNodePtr) malloc(sizeof(LinkedListNode));
  if (newnode == NULL)
    return false;  // out of memory

  newnode->payload = payload;
  newnode->next = iter->node;
  newnode->prev = iter->node->prev;
  newnode->prev->next = newnode;
  newnode->next->prev = newnode;
  iter->list->num_elements += 1;
  return true;
}
