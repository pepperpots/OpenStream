#ifndef LIST_H
#define LIST_H

/***************************************************************************/
/* Implement linked list operations:
 *
 * - structures must contain a pointer to the next element as the
 *   first struct field.
 * - initialization can rely on calloc'ed stream data structure
 * - currently used for implementing streams in cases of
 *   single-threaded control program.
 */
/***************************************************************************/

typedef struct wstream_df_list_element
{
  struct wstream_df_list_element * next;

} wstream_df_list_element_t, *wstream_df_list_element_p;

typedef struct wstream_df_list
{
  wstream_df_list_element_p first;
  wstream_df_list_element_p last;
  wstream_df_list_element_p active_peek_chain;

} wstream_df_list_t, *wstream_df_list_p;

static inline void
wstream_df_list_init (wstream_df_list_p list)
{
  list->first = NULL;
  list->last = NULL;
}

static inline void
wstream_df_list_push (wstream_df_list_p list, void *e)
{
  wstream_df_list_element_p elem = (wstream_df_list_element_p) e;
  wstream_df_list_element_p prev_last;

  elem->next = NULL;

  if (list->first == NULL)
    {
      list->first = elem;
      list->last = elem;
      return;
    }

  prev_last = list->last;
  list->last = elem;
  prev_last->next = elem;
}

static inline wstream_df_list_element_p
wstream_df_list_pop (wstream_df_list_p list)
{
  wstream_df_list_element_p first = list->first;

  /* If the list is empty.  */
  if (first == NULL)
    return NULL;

  if (first->next == NULL)
    list->last = NULL;

  list->first = first->next;
  return first;
}

static inline wstream_df_list_element_p
wstream_df_list_head (wstream_df_list_p list)
{
  return list->first;
}


#endif
