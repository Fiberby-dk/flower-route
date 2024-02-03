/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Userspace adaptation:
 * =============================================================================
 *
 *    Description:  rbtree(Red-Black tree) implementation adapted from linux
 *                  kernel thus can be used in userspace c program.
 *
 *        Created:  09/02/2012 11:36:11 PM
 *
 *         Author:  Fu Haiping (forhappy), haipingf@gmail.com
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

/*
 * Original kernel implementaion:
 * Red Black Trees
 * (C) 1999  Andrea Arcangeli <andrea@suse.de>
 *
 * To use rbtrees you'll have to implement your own insert and search cores.
 * This will avoid us to use callbacks and to drop drammatically performances.
 * I know it's not the cleaner way,  but in C (not in C++) to get
 * performances and genericity...

 * Some example of insert and search follows here. The search is a plain
 * normal search over an ordered tree. The insert instead must be implemented
 * in two steps: First, the code must insert the element in order as a red leaf
 * in the tree, and then the support library function rb_insert_color() must
 * be called. Such function will do the not trivial work to rebalance the
 * rbtree, if necessary.
 *
 * Boilerplate code:
 *
 * static inline struct page *rb_search_page_cache(struct inode *inode,
 *                                                 unsigned long offset)
 * {
 *      struct rb_node *n = inode->i_rb_page_cache.rb_node;
 *      struct page *page;
 *
 *      while (n) {
 *              page = rb_entry(n, struct page, rb_page_cache);
 *
 *              if (offset < page->offset)
 *                      n = n->rb_left;
 *              else if (offset > page->offset)
 *                      n = n->rb_right;
 *              else
 *                      return page;
 *      }
 *      return NULL;
 * }
 *
 * static inline struct page *__rb_insert_page_cache(struct inode *inode,
 *                                                   unsigned long offset,
 *                                                   struct rb_node *node)
 * {
 *      struct rb_node **p = &inode->i_rb_page_cache.rb_node;
 *      struct rb_node *parent = NULL;
 *      struct page *page;
 *
 *      while (*p)
 *      {
 *              parent = *p;
 *              page = rb_entry(parent, struct page, rb_page_cache);
 *
 *              if (offset < page->offset)
 *                      p = &(*p)->rb_left;
 *              else if (offset > page->offset)
 *                      p = &(*p)->rb_right;
 *              else
 *                      return page;
 *      }
 *
 *      rb_link_node(node, parent, p);
 *
 *      return NULL;
 * }
 *
 * static inline struct page *rb_insert_page_cache(struct inode *inode,
 *                                                 unsigned long offset,
 *                                                 struct rb_node *node)
 * {
 *      struct page *ret;
 *      if ((ret = __rb_insert_page_cache(inode, offset, node)))
 *              goto out;
 *      rb_insert_color(node, &inode->i_rb_page_cache);
 *  out:
 *      return ret;
 * }
 */

#include <stddef.h>

#ifndef _LINUX_RBTREE_H
#define _LINUX_RBTREE_H

/* credits: https://stackoverflow.com/questions/10269685/ */
#ifdef __GNUC__
#define rb_member_type(type, member) __typeof__(((type *)0)->member)
#else
#define rb_member_type(type, member) const void
#endif


#define rb_container_of(ptr, type, member) ((type *)( \
	(char *)(rb_member_type(type, member) *) { ptr } -offsetof(type, member)))

enum rb_colors {
	RB_RED   = 0,
	RB_BLACK = 1
};

struct rb_node {
	unsigned long  rb_parent_color;
	struct rb_node *rb_right;
	struct rb_node *rb_left;
};
struct rb_root {
	struct rb_node *rb_node;
};

#define rb_parent(r)    ((struct rb_node *)((r)->rb_parent_color & ~3))
#define rb_color(r)     ((r)->rb_parent_color & 1)
#define rb_is_red(r)    (!rb_color(r))
#define rb_is_black(r)  rb_color(r)
#define rb_set_red(r)   ((r)->rb_parent_color &= ~1)
#define rb_set_black(r) ((r)->rb_parent_color |= 1)

static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)
{
	rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;
}
static inline void rb_set_color(struct rb_node *rb, int color)
{
	rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;
}

#define RB_ROOT { NULL, }
#define rb_entry(ptr, type, member) rb_container_of(ptr, type, member)

#define RB_EMPTY_ROOT(root) ((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node) (rb_parent(node) == node)
#define RB_CLEAR_NODE(node) (rb_set_parent(node, node))

static inline void rb_init_node(struct rb_node *rb)
{
	rb->rb_parent_color = 0;
	rb->rb_right = NULL;
	rb->rb_left = NULL;
	RB_CLEAR_NODE(rb);
}

extern void rb_insert_color(struct rb_node *node, struct rb_root *root);
extern void rb_erase(struct rb_node *node, struct rb_root *root);

typedef void (*rb_augment_f)(struct rb_node *node, void *data);

extern void rb_augment_insert(struct rb_node *node, rb_augment_f func, void *data);
extern struct rb_node *rb_augment_erase_begin(struct rb_node *node);
extern void rb_augment_erase_end(struct rb_node *node, rb_augment_f func, void *data);

/* Find logical next and previous nodes in a tree */
extern struct rb_node *rb_next(const struct rb_node *node);
extern struct rb_node *rb_prev(const struct rb_node *node);
extern struct rb_node *rb_first(const struct rb_root *root);
extern struct rb_node *rb_last(const struct rb_root *root);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rb_replace_node(struct rb_node *victim, struct rb_node *new, struct rb_root *root);

static inline void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **rb_link)
{
	node->rb_parent_color = (unsigned long) parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

#endif	/* _LINUX_RBTREE_H */
