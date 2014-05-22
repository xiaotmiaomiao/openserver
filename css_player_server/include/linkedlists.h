/* 
 * File:   linkedlist.h
 * Author: root
 *
 * Created on April 17, 2014, 8:49 PM
 */


#ifndef LINKEDLIST_H
#define	LINKEDLIST_H

#include "lock.h"

#ifdef	__cplusplus
extern "C" {
#endif


/*!
 * \file linkedlists.h
 * \brief A set of macros to manage forward-linked lists.
 */

/*!
 * \brief Locks a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_LIST_LOCK(head)						\
	css_mutex_lock(&(head)->lock)

/*!
 * \brief Write locks a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive write lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_RWLIST_WRLOCK(head)                                         \
        css_rwlock_wrlock(&(head)->lock)

/*!
 * \brief Write locks a list, with timeout.
 * \param head This is a pointer to the list head structure
 * \param ts Pointer to a timespec structure
 *
 * This macro attempts to place an exclusive write lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 *
 * \since 1.6.1
 */
#define	CSS_RWLIST_TIMEDWRLOCK(head, ts)	css_rwlock_timedwrlock(&(head)->lock, ts)

/*!
 * \brief Read locks a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place a read lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_RWLIST_RDLOCK(head)                                         \
        css_rwlock_rdlock(&(head)->lock)

/*!
 * \brief Read locks a list, with timeout.
 * \param head This is a pointer to the list head structure
 * \param ts Pointer to a timespec structure
 *
 * This macro attempts to place a read lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 *
 * \since 1.6.1
 */
#define CSS_RWLIST_TIMEDRDLOCK(head, ts)                                 \
	css_rwlock_timedrdlock(&(head)->lock, ts)

/*!
 * \brief Locks a list, without blocking if the list is locked.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_LIST_TRYLOCK(head)						\
	css_mutex_trylock(&(head)->lock)

/*!
 * \brief Write locks a list, without blocking if the list is locked.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place an exclusive write lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_RWLIST_TRYWRLOCK(head)                                      \
        css_rwlock_trywrlock(&(head)->lock)

/*!
 * \brief Read locks a list, without blocking if the list is locked.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to place a read lock in the
 * list head structure pointed to by head.
 * \retval 0 on success
 * \retval non-zero on failure
 */
#define CSS_RWLIST_TRYRDLOCK(head)                                      \
        css_rwlock_tryrdlock(&(head)->lock)

/*!
 * \brief Attempts to unlock a list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to remove an exclusive lock from the
 * list head structure pointed to by head. If the list
 * was not locked by this thread, this macro has no effect.
 */
#define CSS_LIST_UNLOCK(head)						\
	css_mutex_unlock(&(head)->lock)

/*!
 * \brief Attempts to unlock a read/write based list.
 * \param head This is a pointer to the list head structure
 *
 * This macro attempts to remove a read or write lock from the
 * list head structure pointed to by head. If the list
 * was not locked by this thread, this macro has no effect.
 */
#define CSS_RWLIST_UNLOCK(head)                                         \
        css_rwlock_unlock(&(head)->lock)

/*!
 * \brief Defines a structure to be used to hold a list of specified type.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type. It does not actually
 * declare (allocate) a structure; to do that, either follow this
 * macro with the desired name of the instance you wish to declare,
 * or use the specified \a name to declare instances elsewhere.
 *
 * Example usage:
 * \code
 * static CSS_LIST_HEAD(entry_list, entry) entries;
 * \endcode
 *
 * This would define \c struct \c entry_list, and declare an instance of it named
 * \a entries, all intended to hold a list of type \c struct \c entry.
 */
#define CSS_LIST_HEAD(name, type)					\
struct name {								\
	struct type *first;						\
	struct type *lcss;						\
	css_mutex_t lock;						\
}

/*!
 * \brief Defines a structure to be used to hold a read/write list of specified type.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type. It does not actually
 * declare (allocate) a structure; to do that, either follow this
 * macro with the desired name of the instance you wish to declare,
 * or use the specified \a name to declare instances elsewhere.
 *
 * Example usage:
 * \code
 * static CSS_RWLIST_HEAD(entry_list, entry) entries;
 * \endcode
 *
 * This would define \c struct \c entry_list, and declare an instance of it named
 * \a entries, all intended to hold a list of type \c struct \c entry.
 */
#define CSS_RWLIST_HEAD(name, type)                                     \
struct name {                                                           \
        struct type *first;                                             \
        struct type *lcss;                                              \
        css_rwlock_t lock;                                              \
}

/*!
 * \brief Defines a structure to be used to hold a list of specified type (with no lock).
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type. It does not actually
 * declare (allocate) a structure; to do that, either follow this
 * macro with the desired name of the instance you wish to declare,
 * or use the specified \a name to declare instances elsewhere.
 *
 * Example usage:
 * \code
 * static CSS_LIST_HEAD_NOLOCK(entry_list, entry) entries;
 * \endcode
 *
 * This would define \c struct \c entry_list, and declare an instance of it named
 * \a entries, all intended to hold a list of type \c struct \c entry.
 */
#define CSS_LIST_HEAD_NOLOCK(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *lcss;						\
}

/*!
 * \brief Defines initial values for a declaration of CSS_LIST_HEAD
 */
#define CSS_LIST_HEAD_INIT_VALUE	{		\
	.first = NULL,					\
	.lcss = NULL,					\
	.lock = CSS_MUTEX_INIT_VALUE,			\
	}

/*!
 * \brief Defines initial values for a declaration of CSS_RWLIST_HEAD
 */
#define CSS_RWLIST_HEAD_INIT_VALUE      {               \
        .first = NULL,                                  \
        .lcss = NULL,                                   \
        .lock = CSS_RWLOCK_INIT_VALUE,                  \
        }

/*!
 * \brief Defines initial values for a declaration of CSS_LIST_HEAD_NOLOCK
 */
#define CSS_LIST_HEAD_NOLOCK_INIT_VALUE	{	\
	.first = NULL,					\
	.lcss = NULL,					\
	}

/*!
 * \brief Defines a structure to be used to hold a list of specified type, statically initialized.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type, and allocates an instance
 * of it, initialized to be empty.
 *
 * Example usage:
 * \code
 * static CSS_LIST_HEAD_STATIC(entry_list, entry);
 * \endcode
 *
 * This would define \c struct \c entry_list, intended to hold a list of
 * type \c struct \c entry.
 */
#if defined(CSS_MUTEX_INIT_W_CONSTRUCTORS)
#define CSS_LIST_HEAD_STATIC(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *lcss;						\
	css_mutex_t lock;						\
} name;									\
static void  __attribute__((constructor)) __init_##name(void)		\
{									\
        CSS_LIST_HEAD_INIT(&name);					\
}									\
static void  __attribute__((destructor)) __fini_##name(void)		\
{									\
        CSS_LIST_HEAD_DESTROY(&name);					\
}									\
struct __dummy_##name
#else
#define CSS_LIST_HEAD_STATIC(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *lcss;						\
	css_mutex_t lock;						\
} name = CSS_LIST_HEAD_INIT_VALUE
#endif

/*!
 * \brief Defines a structure to be used to hold a read/write list of specified type, statically initialized.
 * \param name This will be the name of the defined structure.
 * \param type This is the type of each list entry.
 *
 * This macro creates a structure definition that can be used
 * to hold a list of the entries of type \a type, and allocates an instance
 * of it, initialized to be empty.
 *
 * Example usage:
 * \code
 * static CSS_RWLIST_HEAD_STATIC(entry_list, entry);
 * \endcode
 *
 * This would define \c struct \c entry_list, intended to hold a list of
 * type \c struct \c entry.
 */
#ifndef HAVE_PTHREAD_RWLOCK_INITIALIZER
#define CSS_RWLIST_HEAD_STATIC(name, type)                              \
struct name {                                                           \
        struct type *first;                                             \
        struct type *lcss;                                              \
        css_rwlock_t lock;                                              \
} name;                                                                 \
static void  __attribute__((constructor)) __init_##name(void)          \
{                                                                       \
        CSS_RWLIST_HEAD_INIT(&name);                                    \
}                                                                       \
static void  __attribute__((destructor)) __fini_##name(void)           \
{                                                                       \
        CSS_RWLIST_HEAD_DESTROY(&name);                                 \
}                                                                       \
struct __dummy_##name
#else
#define CSS_RWLIST_HEAD_STATIC(name, type)                              \
struct name {                                                           \
        struct type *first;                                             \
        struct type *lcss;                                              \
        css_rwlock_t lock;                                              \
} name = CSS_RWLIST_HEAD_INIT_VALUE
#endif

/*!
 * \brief Defines a structure to be used to hold a list of specified type, statically initialized.
 *
 * This is the same as CSS_LIST_HEAD_STATIC, except without the lock included.
 */
#define CSS_LIST_HEAD_NOLOCK_STATIC(name, type)				\
struct name {								\
	struct type *first;						\
	struct type *lcss;						\
} name = CSS_LIST_HEAD_NOLOCK_INIT_VALUE

/*!
 * \brief Initializes a list head structure with a specified first entry.
 * \param head This is a pointer to the list head structure
 * \param entry pointer to the list entry that will become the head of the list
 *
 * This macro initializes a list head structure by setting the head
 * entry to the supplied value and recreating the embedded lock.
 */
#define CSS_LIST_HEAD_SET(head, entry) do {				\
	(head)->first = (entry);					\
	(head)->lcss = (entry);						\
	css_mutex_init(&(head)->lock);					\
} while (0)

/*!
 * \brief Initializes an rwlist head structure with a specified first entry.
 * \param head This is a pointer to the list head structure
 * \param entry pointer to the list entry that will become the head of the list
 *
 * This macro initializes a list head structure by setting the head
 * entry to the supplied value and recreating the embedded lock.
 */
#define CSS_RWLIST_HEAD_SET(head, entry) do {                           \
        (head)->first = (entry);                                        \
        (head)->lcss = (entry);                                         \
        css_rwlock_init(&(head)->lock);                                 \
} while (0)

/*!
 * \brief Initializes a list head structure with a specified first entry.
 * \param head This is a pointer to the list head structure
 * \param entry pointer to the list entry that will become the head of the list
 *
 * This macro initializes a list head structure by setting the head
 * entry to the supplied value.
 */
#define CSS_LIST_HEAD_SET_NOLOCK(head, entry) do {			\
	(head)->first = (entry);					\
	(head)->lcss = (entry);						\
} while (0)

/*!
 * \brief Declare a forward link structure inside a list entry.
 * \param type This is the type of each list entry.
 *
 * This macro declares a structure to be used to link list entries together.
 * It must be used inside the definition of the structure named in
 * \a type, as follows:
 *
 * \code
 * struct list_entry {
      ...
      CSS_LIST_ENTRY(list_entry) list;
 * }
 * \endcode
 *
 * The field name \a list here is arbitrary, and can be anything you wish.
 */
#define CSS_LIST_ENTRY(type)						\
struct {								\
	struct type *next;						\
}

#define CSS_RWLIST_ENTRY CSS_LIST_ENTRY

/*!
 * \brief Returns the first entry contained in a list.
 * \param head This is a pointer to the list head structure
 */
#define	CSS_LIST_FIRST(head)	((head)->first)

#define CSS_RWLIST_FIRST CSS_LIST_FIRST

/*!
 * \brief Returns the lcss entry contained in a list.
 * \param head This is a pointer to the list head structure
 */
#define	CSS_LIST_LCSS(head)	((head)->lcss)

#define CSS_RWLIST_LCSS CSS_LIST_LCSS

/*!
 * \brief Returns the next entry in the list after the given entry.
 * \param elm This is a pointer to the current entry.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define CSS_LIST_NEXT(elm, field)	((elm)->field.next)

#define CSS_RWLIST_NEXT CSS_LIST_NEXT

/*!
 * \brief Checks whether the specified list contains any entries.
 * \param head This is a pointer to the list head structure
 *
 * \return zero if the list has entries
 * \return non-zero if not.
 */
#define	CSS_LIST_EMPTY(head)	(CSS_LIST_FIRST(head) == NULL)

#define CSS_RWLIST_EMPTY CSS_LIST_EMPTY

/*!
 * \brief Loops over (traverses) the entries in a list.
 * \param head This is a pointer to the list head structure
 * \param var This is the name of the variable that will hold a pointer to the
 * current list entry on each iteration. It must be declared before calling
 * this macro.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * This macro is use to loop over (traverse) the entries in a list. It uses a
 * \a for loop, and supplies the enclosed code with a pointer to each list
 * entry as it loops. It is typically used as follows:
 * \code
 * static CSS_LIST_HEAD(entry_list, list_entry) entries;
 * ...
 * struct list_entry {
      ...
      CSS_LIST_ENTRY(list_entry) list;
 * }
 * ...
 * struct list_entry *current;
 * ...
 * CSS_LIST_TRAVERSE(&entries, current, list) {
     (do something with current here)
 * }
 * \endcode
 * \warning If you modify the forward-link pointer contained in the \a current entry while
 * inside the loop, the behavior will be unpredictable. At a minimum, the following
 * macros will modify the forward-link pointer, and should not be used inside
 * CSS_LIST_TRAVERSE() against the entry pointed to by the \a current pointer without
 * careful consideration of their consequences:
 * \li CSS_LIST_NEXT() (when used as an lvalue)
 * \li CSS_LIST_INSERT_AFTER()
 * \li CSS_LIST_INSERT_HEAD()
 * \li CSS_LIST_INSERT_TAIL()
 * \li CSS_LIST_INSERT_SORTALPHA()
 */
#define CSS_LIST_TRAVERSE(head,var,field) 				\
	for((var) = (head)->first; (var); (var) = (var)->field.next)

#define CSS_RWLIST_TRAVERSE CSS_LIST_TRAVERSE

/*!
 * \brief Loops safely over (traverses) the entries in a list.
 * \param head This is a pointer to the list head structure
 * \param var This is the name of the variable that will hold a pointer to the
 * current list entry on each iteration. It must be declared before calling
 * this macro.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * This macro is used to safely loop over (traverse) the entries in a list. It
 * uses a \a for loop, and supplies the enclosed code with a pointer to each list
 * entry as it loops. It is typically used as follows:
 *
 * \code
 * static CSS_LIST_HEAD(entry_list, list_entry) entries;
 * ...
 * struct list_entry {
      ...
      CSS_LIST_ENTRY(list_entry) list;
 * }
 * ...
 * struct list_entry *current;
 * ...
 * CSS_LIST_TRAVERSE_SAFE_BEGIN(&entries, current, list) {
     (do something with current here)
 * }
 * CSS_LIST_TRAVERSE_SAFE_END;
 * \endcode
 *
 * It differs from CSS_LIST_TRAVERSE() in that the code inside the loop can modify
 * (or even free, after calling CSS_LIST_REMOVE_CURRENT()) the entry pointed to by
 * the \a current pointer without affecting the loop traversal.
 */
#define CSS_LIST_TRAVERSE_SAFE_BEGIN(head, var, field) {				\
	typeof((head)) __list_head = head;						\
	typeof(__list_head->first) __list_next;						\
	typeof(__list_head->first) __list_prev = NULL;					\
	typeof(__list_head->first) __new_prev = NULL;					\
	for ((var) = __list_head->first, __new_prev = (var),				\
	      __list_next = (var) ? (var)->field.next : NULL;				\
	     (var);									\
	     __list_prev = __new_prev, (var) = __list_next,				\
	     __new_prev = (var),							\
	     __list_next = (var) ? (var)->field.next : NULL,				\
	     (void) __list_prev								\
	    )

#define CSS_RWLIST_TRAVERSE_SAFE_BEGIN CSS_LIST_TRAVERSE_SAFE_BEGIN

/*!
 * \brief Removes the \a current entry from a list during a traversal.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * \note This macro can \b only be used inside an CSS_LIST_TRAVERSE_SAFE_BEGIN()
 * block; it is used to unlink the current entry from the list without affecting
 * the list traversal (and without having to re-traverse the list to modify the
 * previous entry, if any).
 */
#define CSS_LIST_REMOVE_CURRENT(field) do { \
	__new_prev->field.next = NULL;							\
	__new_prev = __list_prev;							\
	if (__list_prev)								\
		__list_prev->field.next = __list_next;					\
	else										\
		__list_head->first = __list_next;					\
	if (!__list_next)								\
		__list_head->lcss = __list_prev;					\
	} while (0)

#define CSS_RWLIST_REMOVE_CURRENT CSS_LIST_REMOVE_CURRENT

#define CSS_LIST_MOVE_CURRENT(newhead, field) do { \
	typeof ((newhead)->first) __list_cur = __new_prev;				\
	CSS_LIST_REMOVE_CURRENT(field);							\
	CSS_LIST_INSERT_TAIL((newhead), __list_cur, field);				\
	} while (0)

#define CSS_RWLIST_MOVE_CURRENT CSS_LIST_MOVE_CURRENT

/*!
 * \brief Inserts a list entry before the current entry during a traversal.
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * \note This macro can \b only be used inside an CSS_LIST_TRAVERSE_SAFE_BEGIN()
 * block.
 */
#define CSS_LIST_INSERT_BEFORE_CURRENT(elm, field) do {			\
	if (__list_prev) {						\
		(elm)->field.next = __list_prev->field.next;		\
		__list_prev->field.next = elm;				\
	} else {							\
		(elm)->field.next = __list_head->first;			\
		__list_head->first = (elm);				\
	}								\
	__new_prev = (elm);						\
} while (0)

#define CSS_RWLIST_INSERT_BEFORE_CURRENT CSS_LIST_INSERT_BEFORE_CURRENT

/*!
 * \brief Closes a safe loop traversal block.
 */
#define CSS_LIST_TRAVERSE_SAFE_END  }

#define CSS_RWLIST_TRAVERSE_SAFE_END CSS_LIST_TRAVERSE_SAFE_END

/*!
 * \brief Initializes a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro initializes a list head structure by setting the head
 * entry to \a NULL (empty list) and recreating the embedded lock.
 */
#define CSS_LIST_HEAD_INIT(head) {					\
	(head)->first = NULL;						\
	(head)->lcss = NULL;						\
	css_mutex_init(&(head)->lock);					\
}

/*!
 * \brief Initializes an rwlist head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro initializes a list head structure by setting the head
 * entry to \a NULL (empty list) and recreating the embedded lock.
 */
#define CSS_RWLIST_HEAD_INIT(head) {                                    \
        (head)->first = NULL;                                           \
        (head)->lcss = NULL;                                            \
        css_rwlock_init(&(head)->lock);                                 \
}

/*!
 * \brief Destroys a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro destroys a list head structure by setting the head
 * entry to \a NULL (empty list) and destroying the embedded lock.
 * It does not free the structure from memory.
 */
#define CSS_LIST_HEAD_DESTROY(head) {					\
	(head)->first = NULL;						\
	(head)->lcss = NULL;						\
	css_mutex_destroy(&(head)->lock);				\
}

/*!
 * \brief Destroys an rwlist head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro destroys a list head structure by setting the head
 * entry to \a NULL (empty list) and destroying the embedded lock.
 * It does not free the structure from memory.
 */
#define CSS_RWLIST_HEAD_DESTROY(head) {                                 \
        (head)->first = NULL;                                           \
        (head)->lcss = NULL;                                            \
        css_rwlock_destroy(&(head)->lock);                              \
}

/*!
 * \brief Initializes a list head structure.
 * \param head This is a pointer to the list head structure
 *
 * This macro initializes a list head structure by setting the head
 * entry to \a NULL (empty list). There is no embedded lock handling
 * with this macro.
 */
#define CSS_LIST_HEAD_INIT_NOLOCK(head) {				\
	(head)->first = NULL;						\
	(head)->lcss = NULL;						\
}

/*!
 * \brief Inserts a list entry after a given entry.
 * \param head This is a pointer to the list head structure
 * \param listelm This is a pointer to the entry after which the new entry should
 * be inserted.
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define CSS_LIST_INSERT_AFTER(head, listelm, elm, field) do {		\
	(elm)->field.next = (listelm)->field.next;			\
	(listelm)->field.next = (elm);					\
	if ((head)->lcss == (listelm))					\
		(head)->lcss = (elm);					\
} while (0)

#define CSS_RWLIST_INSERT_AFTER CSS_LIST_INSERT_AFTER

/*!
 * \brief Inserts a list entry at the head of a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be inserted.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 */
#define CSS_LIST_INSERT_HEAD(head, elm, field) do {			\
		(elm)->field.next = (head)->first;			\
		(head)->first = (elm);					\
		if (!(head)->lcss)					\
			(head)->lcss = (elm);				\
} while (0)

#define CSS_RWLIST_INSERT_HEAD CSS_LIST_INSERT_HEAD

/*!
 * \brief Appends a list entry to the tail of a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be appended.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * Note: The link field in the appended entry is \b not modified, so if it is
 * actually the head of a list itself, the entire list will be appended
 * temporarily (until the next CSS_LIST_INSERT_TAIL is performed).
 */
#define CSS_LIST_INSERT_TAIL(head, elm, field) do {			\
      if (!(head)->first) {						\
		(head)->first = (elm);					\
		(head)->lcss = (elm);					\
      } else {								\
		(head)->lcss->field.next = (elm);			\
		(head)->lcss = (elm);					\
      }									\
} while (0)

#define CSS_RWLIST_INSERT_TAIL CSS_LIST_INSERT_TAIL

/*!
 * \brief Inserts a list entry into a alphabetically sorted list
 * \param head Pointer to the list head structure
 * \param elm Pointer to the entry to be inserted
 * \param field Name of the list entry field (declared using CSS_LIST_ENTRY())
 * \param sortfield Name of the field on which the list is sorted
 * \since 1.6.1
 */
#define CSS_LIST_INSERT_SORTALPHA(head, elm, field, sortfield) do { \
	if (!(head)->first) {                                           \
		(head)->first = (elm);                                      \
		(head)->lcss = (elm);                                       \
	} else {                                                        \
		typeof((head)->first) cur = (head)->first, prev = NULL;     \
		while (cur && strcmp(cur->sortfield, elm->sortfield) < 0) { \
			prev = cur;                                             \
			cur = cur->field.next;                                  \
		}                                                           \
		if (!prev) {                                                \
			CSS_LIST_INSERT_HEAD(head, elm, field);                 \
		} else if (!cur) {                                          \
			CSS_LIST_INSERT_TAIL(head, elm, field);                 \
		} else {                                                    \
			CSS_LIST_INSERT_AFTER(head, prev, elm, field);          \
		}                                                           \
	}                                                               \
} while (0)

#define CSS_RWLIST_INSERT_SORTALPHA	CSS_LIST_INSERT_SORTALPHA

/*!
 * \brief Appends a whole list to the tail of a list.
 * \param head This is a pointer to the list head structure
 * \param list This is a pointer to the list to be appended.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * Note: The source list (the \a list parameter) will be empty after
 * calling this macro (the list entries are \b moved to the target list).
 */
#define CSS_LIST_APPEND_LIST(head, list, field) do {			\
	if (!(list)->first) {						\
		break;							\
	}								\
	if (!(head)->first) {						\
		(head)->first = (list)->first;				\
		(head)->lcss = (list)->lcss;				\
	} else {							\
		(head)->lcss->field.next = (list)->first;		\
		(head)->lcss = (list)->lcss;				\
	}								\
	(list)->first = NULL;						\
	(list)->lcss = NULL;						\
} while (0)

#define CSS_RWLIST_APPEND_LIST CSS_LIST_APPEND_LIST

/*!
  \brief Inserts a whole list after a specific entry in a list
  \param head This is a pointer to the list head structure
  \param list This is a pointer to the list to be inserted.
  \param elm This is a pointer to the entry after which the new list should
  be inserted.
  \param field This is the name of the field (declared using CSS_LIST_ENTRY())
  used to link entries of the lists together.

  Note: The source list (the \a list parameter) will be empty after
  calling this macro (the list entries are \b moved to the target list).
 */
#define CSS_LIST_INSERT_LIST_AFTER(head, list, elm, field) do {		\
	(list)->lcss->field.next = (elm)->field.next;			\
	(elm)->field.next = (list)->first;				\
	if ((head)->lcss == elm) {					\
		(head)->lcss = (list)->lcss;				\
	}								\
	(list)->first = NULL;						\
	(list)->lcss = NULL;						\
} while(0)

#define CSS_RWLIST_INSERT_LIST_AFTER CSS_LIST_INSERT_LIST_AFTER

/*!
 * \brief Removes and returns the head entry from a list.
 * \param head This is a pointer to the list head structure
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 *
 * Removes the head entry from the list, and returns a pointer to it.
 * This macro is safe to call on an empty list.
 */
#define CSS_LIST_REMOVE_HEAD(head, field) ({				\
		typeof((head)->first) cur = (head)->first;		\
		if (cur) {						\
			(head)->first = cur->field.next;		\
			cur->field.next = NULL;				\
			if ((head)->lcss == cur)			\
				(head)->lcss = NULL;			\
		}							\
		cur;							\
	})

#define CSS_RWLIST_REMOVE_HEAD CSS_LIST_REMOVE_HEAD

/*!
 * \brief Removes a specific entry from a list.
 * \param head This is a pointer to the list head structure
 * \param elm This is a pointer to the entry to be removed.
 * \param field This is the name of the field (declared using CSS_LIST_ENTRY())
 * used to link entries of this list together.
 * \warning The removed entry is \b not freed nor modified in any way.
 */
#define CSS_LIST_REMOVE(head, elm, field) ({			        \
	__typeof(elm) __res = NULL; \
	__typeof(elm) __tmp = elm; \
	if (!__tmp) { \
		__res = NULL; \
	} else if ((head)->first == (elm)) {					\
		__res = (head)->first;                      \
		(head)->first = (elm)->field.next;			\
		if ((head)->lcss == (elm))			\
			(head)->lcss = NULL;			\
	} else {								\
		typeof(elm) curelm = (head)->first;			\
		while (curelm && (curelm->field.next != (elm)))			\
			curelm = curelm->field.next;			\
		if (curelm) { \
			__res = (elm); \
			curelm->field.next = (elm)->field.next;			\
			if ((head)->lcss == (elm))				\
				(head)->lcss = curelm;				\
		} \
	}								\
	if (__res) { \
		(__res)->field.next = NULL; \
	} \
	(__res); \
})

#define CSS_RWLIST_REMOVE CSS_LIST_REMOVE

#ifdef	__cplusplus
}
#endif

#endif	/* LINKEDLIST_H */

