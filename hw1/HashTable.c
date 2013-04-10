/*
 * Copyright 2011 Steven Gribble
 *
 *  This file is part of the UW CSE 333 course project sequence
 *  (333proj).
 *
 *  333proj is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  333proj is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with 333proj.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "Assert333.h"
#include "HashTable.h"
#include "HashTable_priv.h"

// A private utility function to grow the hashtable (increase
// the number of buckets) if its load factor has become too high.
static void ResizeHashtable(HashTable ht);

// a free function that does nothing
static void NullFree(void *freeme) { }

HashTable AllocateHashTable(uint32_t num_buckets) {
  HashTable ht;
  uint32_t  i;

  // defensive programming
  if (num_buckets == 0) {
    return NULL;
  }

  // allocate the hash table record
  ht = (HashTable) malloc(sizeof(HashTableRecord));
  if (ht == NULL) {
    return NULL;
  }

  // initialize the record
  ht->num_buckets = num_buckets;
  ht->num_elements = 0;
  ht->buckets =
    (LinkedList *) malloc(num_buckets * sizeof(LinkedList));
  if (ht->buckets == NULL) {
    // make sure we don't leak!
    free(ht);
    return NULL;
  }
  for (i = 0; i < num_buckets; i++) {
    ht->buckets[i] = AllocateLinkedList();
    if (ht->buckets[i] == NULL) {
      // allocating one of our bucket chain lists failed,
      // so we need to free everything we allocated so far
      // before returning NULL to indicate failure.  Since
      // we know the chains are empty, we'll pass in a
      // free function pointer that does nothing; it should
      // never be called.
      uint32_t j;
      for (j = 0; j < i; j++) {
        FreeLinkedList(ht->buckets[j], NullFree);
      }
      free(ht);
      return NULL;
    }
  }

  return (HashTable) ht;
}

void FreeHashTable(HashTable table,
                   ValueFreeFnPtr value_free_function) {
  uint32_t i;

  Assert333(table != NULL);  // be defensive

  // loop through and free the chains on each bucket
  for (i = 0; i < table->num_buckets; i++) {
    LinkedList  bl = table->buckets[i];
    HTKeyValue *nextKV;

    // pop elements off the the chain list, then free the list
    while (NumElementsInLinkedList(bl) > 0) {
      Assert333(PopLinkedList(bl, (void **) &nextKV));
      value_free_function(nextKV->value);
      free(nextKV);
    }
    // the chain list is empty, so we can pass in the
    // null free function to FreeLinkedList.
    FreeLinkedList(bl, NullFree);
  }

  // free the bucket array within the table record,
  // then free the table record itself.
  free(table->buckets);
  free(table);
}

uint64_t NumElementsInHashTable(HashTable table) {
  Assert333(table != NULL);
  return table->num_elements;
}

uint64_t FNVHash64(unsigned char *buffer, unsigned int len) {
  // This code is adapted from code by Landon Curt Noll
  // and Bonelli Nicola:
  //
  // http://code.google.com/p/nicola-bonelli-repo/
  static const uint64_t FNV1_64_INIT = 0xcbf29ce484222325ULL;
  static const uint64_t FNV_64_PRIME = 0x100000001b3ULL;
  unsigned char *bp = (unsigned char *) buffer;
  unsigned char *be = bp + len;
  uint64_t hval = FNV1_64_INIT;

  /*
   * FNV-1a hash each octet of the buffer
   */
  while (bp < be) {
    /* xor the bottom with the current octet */
    hval ^= (uint64_t) * bp++;
    /* multiply by the 64 bit FNV magic prime mod 2^64 */
    hval *= FNV_64_PRIME;
  }
  /* return our new hash value */
  return hval;
}

uint64_t FNVHashInt64(uint64_t hashme) {
  unsigned char buf[8];
  int i;

  for (i = 0; i < 8; i++) {
    buf[i] = (unsigned char) (hashme & 0x00000000000000FFULL);
    hashme >>= 8;
  }
  return FNVHash64(buf, 8);
}

uint64_t HashKeyToBucketNum(HashTable ht, uint64_t key) {
  return key % ht->num_buckets;
}

int InsertHashTable(HashTable table,
                    HTKeyValue newkeyvalue,
                    HTKeyValue *oldkeyvalue) {
  uint32_t insertbucket;
  LinkedList insertchain;

  Assert333(table != NULL);
  ResizeHashtable(table);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HashKeyToBucketNum(table, newkeyvalue.key);
  insertchain = table->buckets[insertbucket];
	Assert333(insertchain != NULL);

	// prep the new element to insert to the chain
	HTKeyValuePtr payload_ptr = (HTKeyValuePtr) malloc(sizeof(HTKeyValue));
	if (payload_ptr == NULL) {
		// allocation failed; return failure
		return 0;
	}
	
	// assign appropriate values for the new HTKeyValue to insert
	payload_ptr->key = newkeyvalue.key;
	payload_ptr->value = newkeyvalue.value;

	if (NumElementsInLinkedList(insertchain) == 0) {
		// empty chain; no need to search for recurring key
		if (AppendLinkedList(insertchain, (void *) payload_ptr)) {
			// append success; increment num_elements and return success
			table->num_elements++;
			return 1;
		} else {
			// append failed; prevent memory leak and return failure
			free(payload_ptr);
			payload_ptr = NULL;
			return 0;
		}
	} else {
		// chain has >= 1 elements; search for recurring key before insert
		HTKeyValue *recurringkeyvalue;
		int result = LookupKey(insertchain, newkeyvalue.key, &recurringkeyvalue, false);
		if (result == -1) {
			// error while looking up key; prevent memory leak and return failure
			free(payload_ptr);
			payload_ptr = NULL;
			return 0;
		} else if (result == 0) {
			// no existing key/value with that key; append new keyvalue to list
			if (AppendLinkedList(insertchain, (void *) payload_ptr)) {
				// append success; increment num_elements and return success
				table->num_elements++;
				return 1;
			} else {
				// append failed; return failure and prevent memory leak
				free(payload_ptr);
				payload_ptr = NULL;
				return 0;
			}
		} else {
			// found existing key/value with that key; replace keyvalue
			*oldkeyvalue = *recurringkeyvalue;
			recurringkeyvalue->value = (void *) newkeyvalue.value;
			
			free(payload_ptr);
			payload_ptr = NULL;
			return 2;
		}
	}
}

int LookupKey(LinkedList chain, uint64_t key, HTKeyValue **resultkeyvalue, bool removeonfind) {
	// make an iterator pointed to the head of the list
	LLIter iter = LLMakeIterator(chain, 0);

	if (iter == NULL) {
		// failed to create iterator; return failure
		return -1;
	}

	// iterate through the bucket to find the element with the specified key
	LLIteratorGetPayload(iter, (void **) resultkeyvalue);
	while ((*resultkeyvalue)->key != key) {
		if (!LLIteratorNext(iter)) {
			// searched through all of the bucket; return not found
			LLIteratorFree(iter);
			iter = NULL;
			return 0;
		} else {
			LLIteratorGetPayload(iter, (void **) resultkeyvalue);
		}
	}

	// optionally remove the found element
	if (removeonfind) {
		LLIteratorDelete(iter, NullFree);
	}

	// return found
	LLIteratorFree(iter);
	iter = NULL;
	return 1;
}

int LookupHashTable(HashTable table,
                    uint64_t key,
                    HTKeyValue *keyvalue) {
  uint32_t insertbucket;
  LinkedList insertchain;
	HTKeyValue *resultkeyvalue;
  Assert333(table != NULL);

  // calculate which bucket we're inserting into,
  // grab its linked list chain
  insertbucket = HashKeyToBucketNum(table, key);
  insertchain = table->buckets[insertbucket];
	Assert333(insertchain != NULL);

	if (NumElementsInLinkedList(insertchain) == 0) {
		// empty chain; return not found
		return 0;
	} else {
		// chain has >= 1 elements; search the chain
		int result = LookupKey(insertchain, key, &resultkeyvalue, false);
		// copy the payload if found
		if (result == 1)
			*keyvalue = *resultkeyvalue;
		return result;
	}
}

int RemoveFromHashTable(HashTable table,
                        uint64_t key,
                        HTKeyValue *keyvalue) {
  uint32_t insertbucket;
  LinkedList insertchain;
	HTKeyValue *resultkeyvalue;
  Assert333(table != NULL);

	// calculate which bucket we're inserting into,
	// grab its linked list chain
	insertbucket = HashKeyToBucketNum(table, key);
	insertchain = table->buckets[insertbucket];
	Assert333(insertchain != NULL);

	if (NumElementsInLinkedList(insertchain) == 0) {
		// nothing to remove; return not found
		return 0;
	} else {
		// chain has >= elements; search the chain and remove
		int result = LookupKey(insertchain, key, &resultkeyvalue, true);
		// copy/free the payload if remove was successful
		if (result == 1) {
			*keyvalue = *resultkeyvalue;
			free(resultkeyvalue);
			resultkeyvalue = NULL;
			table->num_elements--;
		}
		return result;
	}
}

HTIter HashTableMakeIterator(HashTable table) {
  HTIterRecord *iter;
  uint32_t      i;

  Assert333(table != NULL);  // be defensive

  // malloc the iterator
  iter = (HTIterRecord *) malloc(sizeof(HTIterRecord));
  if (iter == NULL) {
    return NULL;
  }

  // if the hash table is empty, the iterator is immediately invalid,
  // since it can't point to anything.
  if (table->num_elements == 0) {
    iter->is_valid = false;
    iter->ht = table;
    iter->bucket_it = NULL;
    return iter;
  }

  // initialize the iterator.  there is at least one element in the
  // table, so find the first element and point the iterator at it.
  iter->is_valid = true;
  iter->ht = table;
  for (i = 0; i < table->num_buckets; i++) {
    if (NumElementsInLinkedList(table->buckets[i]) > 0) {
      iter->bucket_num = i;
      break;
    }
  }
  Assert333(i < table->num_buckets);  // make sure we found it.
  iter->bucket_it = LLMakeIterator(table->buckets[iter->bucket_num], 0UL);
  if (iter->bucket_it == NULL) {
    // out of memory!
    free(iter);
    return NULL;
  }
  return iter;
}

void HTIteratorFree(HTIter iter) {
  Assert333(iter != NULL);
  if (iter->bucket_it != NULL) {
    LLIteratorFree(iter->bucket_it);
    iter->bucket_it = NULL;
  }
  iter->is_valid = false;
  free(iter);
}

int HTIteratorNext(HTIter iter) {
  Assert333(iter != NULL);
	uint32_t i;

	// check that the table is not empty/iterator is not past end
	if (HTIteratorPastEnd(iter) == 1) {
		iter->is_valid = false;
		if (iter->bucket_it != NULL) {
			LLIteratorFree(iter->bucket_it);
    	iter->bucket_it = NULL;
		}
		return 0;
	}

	if (LLIteratorHasNext(iter->bucket_it)) {
		// general case; there are elements in the current bucket to move on to
		Assert333(LLIteratorNext(iter->bucket_it));
		return 1;
	}	

	// iterator points to the tail of the current bucket
  for (i = iter->bucket_num + 1; i < iter->ht->num_buckets; i++) {
    if (NumElementsInLinkedList(iter->ht->buckets[i]) > 0) {
      iter->bucket_num = i;
      break;
    }
	}

	// the old iterator must be freed
	LLIteratorFree(iter->bucket_it);

	if (i >= iter->ht->num_buckets) {
		// degenerate case; the iterator has advanced past of the hash table; set invalid
		iter->is_valid = false;
    iter->bucket_it = NULL;
    return 0;
	} else {
		// general case; the iterator moves onto the next bucket
		iter->bucket_num = i;
	  iter->bucket_it = LLMakeIterator(iter->ht->buckets[iter->bucket_num], 0UL);
  	if (iter->bucket_it == NULL) {
    	// out of memory!
			iter->is_valid = false;
    	return 0;
 		}

		// return success
  	return 1;
	}
}

int HTIteratorPastEnd(HTIter iter) {
  Assert333(iter != NULL);

	if (iter->ht->num_elements == 0) {
		// empty table; return past end
		return 1;
	}

	// only invalid iterators are past the end of the table
	if (iter->is_valid) {
		return 0;
	}	else {
		return 1;
	}
}

int HTIteratorGet(HTIter iter, HTKeyValue *keyvalue) {
	HTKeyValue *payload;
  Assert333(iter != NULL);

	// check to see if table is empty or iterator is invalid
	if (HTIteratorPastEnd(iter) == 1) {
		return 0;
	}

	// get the payload and copy the values into keyvalue
	LLIteratorGetPayload(iter->bucket_it,(void **) &payload);
	*keyvalue = *payload;
  return 1;
}

int HTIteratorDelete(HTIter iter, HTKeyValue *keyvalue) {
  HTKeyValue kv;
  int res, retval;

  Assert333(iter != NULL);

  // Try to get what the iterator is pointing to.
  res = HTIteratorGet(iter, &kv);
  if (res == 0)
    return 0;

  // Advance the iterator.
  res = HTIteratorNext(iter);
  if (res == 0) {
    retval = 2;
  } else {
    retval = 1;
  }
  res = RemoveFromHashTable(iter->ht, kv.key, keyvalue);
  Assert333(res == 1);
  Assert333(kv.key == keyvalue->key);
  Assert333(kv.value == keyvalue->value);

  return retval;
}

static void ResizeHashtable(HashTable ht) {
  // Resize if the load factor is > 3.
  if (ht->num_elements < 3 * ht->num_buckets)
    return;

  // This is the resize case.  Allocate a new hashtable,
  // iterate over the old hashtable, do the surgery on
  // the old hashtable record and free up the new hashtable
  // record.
  HashTable newht = AllocateHashTable(ht->num_buckets * 9);

  // Give up if out of memory.
  if (newht == NULL)
    return;

  // Loop through the old ht with an iterator,
  // inserting into the new HT.
  HTIter it = HashTableMakeIterator(ht);
  if (it == NULL) {
    // Give up if out of memory.
    FreeHashTable(newht, &NullFree);
    return;
  }

  while (!HTIteratorPastEnd(it)) {
    HTKeyValue item, dummy;

    Assert333(HTIteratorGet(it, &item) == 1);
    if (InsertHashTable(newht, item, &dummy) != 1) {
      // failure, free up everything, return.
      HTIteratorFree(it);
      FreeHashTable(newht, &NullFree);
      return;
    }
    HTIteratorNext(it);
  }

  // Worked!  Free the iterator.
  HTIteratorFree(it);

  // Sneaky: swap the structures, then free the new table,
  // and we're done.
  {
    HashTableRecord tmp;

    tmp = *ht;
    *ht = *newht;
    *newht = tmp;
    FreeHashTable(newht, &NullFree);
  }

  return;
}
