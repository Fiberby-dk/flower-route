// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj_rule.h"
#include "obj_target.h"
#include "sched_basic.h"
#include "nl_queue.h"
#include "hexdump.h"
#include "tc_action.h"

static struct rb_root obj_rule_pos_tree = RB_ROOT; /* positional */
static struct rb_root obj_rule_laf_tree = RB_ROOT; /* lost and found */
static int obj_rule_pin_changes; /* TODO change to enum */
static int obj_rule_cnt;

/*
 * the positional tree is used for stuff once we want it to be there
 * the lost and found tree is used to briefly keep track of objects
 * found in the kernel, until requested (or removed), and is used
 * for maintaining stability across process restarts
 */

int obj_rule_count(void)
{
	return obj_rule_cnt;
}

void obj_rule_unset_target(struct obj_rule *r)
{
	obj_assert_kind(r, RULE);
	if (r->target) {
		obj_target_weak_unref(r->target);
		r->target = NULL;
	}
}

static void obj_rule_reap(struct obj_rule *r)
{
	AN(r->obj.refcnt == 0);
	r->obj.state = OBJ_STATE_ZOMBIE;
	obj_rule_unset_target(r);
	if (r->want) {
		free(r->want);
		r->want = NULL;
	}
	if (r->have) {
		free(r->have);
		r->have = NULL;
	}
	if (r->have_laf) {
		rb_erase(&r->laf_node, &obj_rule_laf_tree);
		r->have_laf = false;
	}
	if (r->have_pos) {
		rb_erase(&r->pos_node, &obj_rule_pos_tree);
		r->have_pos = false;
	}
	obj_free(r);
	AN(obj_rule_cnt--);
}

/* define helpers, after obj_rule_reap */
obj_generic_ref(rule, RULE)

void obj_rule_unref(struct obj_rule *r)
{
	obj_assert_kind(r, RULE);
	obj_unref(&r->obj);
	if (r->obj.refcnt == 0 && r->want && r->have &&
	    obj_get_operating_mode() == OBJ_MODE_NORMAL &&
		 r->type != OBJ_RULE_TYPE_STATIC) {
		obj_rule_uninstall(r);
	}
	if (obj_is_reapable(&r->obj))
		obj_rule_reap(r);
}

void obj_rule_set_target(struct obj_rule *r, struct obj_target *t)
{
	obj_assert_kind(r, RULE);
	obj_assert_kind(t, TARGET);
	AZ(r->target);
	r->target = obj_target_weak_ref(t);
}

static int u16cmp(const uint16_t a, const uint16_t b)
{
	return ((int) a) - b;
}

static int u32cmp(const uint32_t a, const uint32_t b)
{
	return ((int) a) - b;
}

static struct obj_rule *obj_rule_laf_lookup(const struct tc_rule *tcr)
{
	struct rb_node *node = obj_rule_laf_tree.rb_node;

	while (node) {
		struct obj_rule *this = rb_container_of(node, struct obj_rule, laf_node);
		int ret = memcmp(tcr, this->have, sizeof(struct tc_rule));

		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

static int obj_rule_laf_insert(struct obj_rule *r)
{
	AN(r->have_laf == false);

	struct rb_node **new = &(obj_rule_laf_tree.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct obj_rule *this = rb_container_of(*new, struct obj_rule, laf_node);
		int ret = memcmp(r->have, this->have, sizeof(struct tc_rule));

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&r->laf_node, parent, new);
	rb_insert_color(&r->laf_node, &obj_rule_laf_tree);

	r->have_laf = true;

	return 1;
}

struct obj_rule *obj_rule_pos_lookup(const uint32_t chain_no, const uint16_t prio)
{
	struct rb_node *node = obj_rule_pos_tree.rb_node;

	while (node) {
		struct obj_rule *this = rb_container_of(node, struct obj_rule, pos_node);
		int ret = u32cmp(chain_no, this->chain_no);

		if (ret == 0)
			ret = u16cmp(prio, this->prio);
		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

static int obj_rule_pos_insert(struct obj_rule *r)
{
	AN(r->have_pos == false);

	struct rb_node **new = &(obj_rule_pos_tree.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct obj_rule *this = rb_container_of(*new, struct obj_rule, pos_node);
		int ret = u32cmp(r->chain_no, this->chain_no);

		if (ret == 0)
			ret = u16cmp(r->prio, this->prio);

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&r->pos_node, parent, new);
	rb_insert_color(&r->pos_node, &obj_rule_pos_tree);

	r->have_pos = true;

	return 1;
}

static void obj_rule_pre_install(void *data)
{
	struct obj_rule *r = data;

	AN(r->state == OBJ_RULE_STATE_QUEUED);
	r->state = OBJ_RULE_STATE_PENDING;
}

static void obj_rule_post_install(void *data)
{
	struct obj_rule *r = data;

	obj_rule_unref(r);
}

static void obj_rule_queue_install(struct obj_rule *r)
{
	if (obj_rule_pin_changes < 2)
		return;

	AN(r->state == OBJ_RULE_STATE_WANT);
	r->state = OBJ_RULE_STATE_QUEUED;
	fr_printf(INFO, "TRYING TO INSTALL RULE 1 (%d,%d)\n", r->chain_no, r->prio);

	tc_action_install(r->chain_no, r->prio, r->want, obj_rule_ref(r));
	fr_printf(DEBUG2, "%s\t%d\n", __func__, r->state);
}

static void obj_rule_queue_uninstall(struct obj_rule *r)
{
	if (obj_rule_pin_changes < 3)
		return;
	AN(r->state == OBJ_RULE_STATE_ALIEN);
	r->state = OBJ_RULE_STATE_QUEUED;
	fr_printf(INFO, "TRYING TO UNINSTALL RULE 1\t%d\t%d\n", r->chain_no, r->prio);
	//if (r->chain_no != 0 && r->chain_no != 4 && r->chain_no != 6) {
	tc_action_install(r->chain_no, r->prio, NULL, obj_rule_ref(r));
	/* uninstall update should trigger removal and new install */
	//}
	/* TODO add error handler */
}

static void obj_rule_update_state(struct obj_rule *r)
{
	if (obj_rule_pin_changes == 0)
		return;
	if (r->state == OBJ_RULE_STATE_QUEUED)
		return; /* XXX */
	if (r->want == NULL && r->have == NULL) {
		if (r->state != OBJ_RULE_STATE_NEW)
			r->state = OBJ_RULE_STATE_ZOMBIE;
	} else if (r->want == NULL) {
		r->state = OBJ_RULE_STATE_ALIEN;
		obj_rule_queue_uninstall(r);
	} else if (r->have == NULL) {
		r->state = OBJ_RULE_STATE_WANT;
		obj_rule_queue_install(r);
	} else if (memcmp(r->want, r->have, sizeof(struct tc_rule)) == 0) {
		r->state = OBJ_RULE_STATE_OK;
		if (r->target)
			obj_target_notify_routes(r->target);
	} else {
		r->state = OBJ_RULE_STATE_ALIEN;
		obj_rule_queue_uninstall(r);
	}
}

void obj_rule_reset_pin(void)
{
	obj_rule_pin_changes = 0;
}

void obj_rule_remove_pin(void)
{
	if (obj_rule_pin_changes >= 3)
		return;
	for (int i = 0; i < 4; i++) {
		//obj_rule_print_all();
		fr_printf(INFO, "removing pin %d\n", i);
		obj_rule_pin_changes = i;

		for (struct rb_node *n = rb_first(&obj_rule_pos_tree); n; n = rb_next(n)) {
			struct obj_rule *r = rb_container_of(n, struct obj_rule, pos_node);

			obj_rule_update_state(r);
		}
	}
}

static void obj_rule_new(struct obj_rule *r)
{
	obj_rule_pos_insert(r);
	obj_rule_laf_insert(r);
	obj_rule_update_state(r);
	//obj_rule_print(r);
}

static void obj_rule_update(struct obj_rule *r)
{
	obj_rule_update_state(r);
}

static void obj_rule_delete(struct obj_rule *r)
{
	obj_rule_ref(r);
	obj_set_state(rule, r, PRESENT);
	if (r->have) {
		free(r->have);
		r->have = NULL;
	}
	if (r->want) {
		/* if have a new want then request it, and don't unref yet */
		obj_rule_update_state(r);
	}
	obj_rule_unref(r);
}

static struct obj_rule *obj_rule_alloc(void)
{
	struct obj_rule *r = fr_malloc(sizeof(struct obj_rule));

	obj_set_kind(r, RULE);
	obj_rule_cnt++;
	return r;
}

void obj_rule_netlink_found(const uint16_t nlmsg_type, const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr)
{
	if (tcr)
		tc_rule_print(tcr);

	struct obj_rule *r = obj_rule_pos_lookup(chain_no, prio);

	if (r == NULL) {
		r = obj_rule_laf_lookup(tcr);
		if (r && (r->chain_no != chain_no || r->prio != prio))
			r = NULL;
	}

	if (r)
		obj_assert_kind(r, RULE);

	if (nlmsg_type == RTM_DELTFILTER) {
		if (r)
			obj_rule_delete(r);
		return;
	}
	AN(tcr);

	int is_new = r == NULL;

	if (is_new) {
		r = obj_rule_alloc();
		r->chain_no = chain_no;
		r->prio = prio;
	} else {
		AN(r->chain_no == chain_no);
		AN(r->prio == prio);
	}

	int changes = 0;

	if (r->have == NULL || memcmp(r->have, tcr, sizeof(struct tc_rule)) != 0) {
		if (r->have)
			free(r->have);
		else
			obj_set_state(rule, r, INSTALLED);
		r->have = fr_malloc(sizeof(struct tc_rule));
		memcpy(r->have, tcr, sizeof(struct tc_rule));
		changes++;
	}

	if (is_new)
		obj_rule_new(r);
	else if (changes > 0)
		obj_rule_update(r);
}

void obj_rule_static_want(const uint32_t chain_no, const uint16_t prio, const struct tc_rule *tcr)
{
	struct obj_rule *r = obj_rule_pos_lookup(chain_no, prio);

	AN(r == NULL);
	r = obj_rule_alloc();
	r->chain_no = chain_no;
	r->prio = prio;
	r->type = OBJ_RULE_TYPE_STATIC;
	AN(tcr);
	r->want = fr_malloc(sizeof(struct tc_rule));
	memcpy(r->want, tcr, sizeof(struct tc_rule));
	obj_rule_pos_insert(r);
	obj_rule_update_state(r);
}

void obj_rule_uninstall(struct obj_rule *r)
{
	if (r->want) {
		free(r->want);
		r->want = NULL;
	}
	obj_rule_update_state(r);
}

struct obj_rule *obj_rule_prime_request(const struct tc_rule *tcr)
{
	struct obj_rule *r = obj_rule_laf_lookup(tcr);

	if (r) {
		/* rule was found in the lost and found tree */
		obj_rule_ref(r);
		AN(r->have != NULL);
		AN(r->want == NULL);
		r->want = fr_malloc(sizeof(struct tc_rule));
		memcpy(r->want, tcr, sizeof(struct tc_rule));
		AN(r->have_laf == true);
		rb_erase(&r->laf_node, &obj_rule_laf_tree);
		r->have_laf = false;
		return obj_rule_ref(r);
	}

	uint32_t chain_no = 0;
	uint16_t prio = 0;
	int ret = sched_place(tcr, &chain_no, &prio);

	if (ret) {
		struct obj_rule *r = obj_rule_ref(obj_rule_alloc());

		r->chain_no = chain_no;
		r->prio = prio;
		r->want = fr_malloc(sizeof(struct tc_rule));
		memcpy(r->want, tcr, sizeof(struct tc_rule));
		obj_rule_pos_insert(r);
		return r;
	}

	return NULL;
}

void obj_rule_queue_request(struct obj_rule *r)
{
	obj_rule_update_state(r);
}

struct obj_rule *obj_rule_request(const struct tc_rule *tcr)
{
	struct obj_rule *r = obj_rule_prime_request(tcr);

	if (r)
		obj_rule_queue_request(r);
	return r;
}

void obj_rule_print_all(void)
{
	for (struct rb_node *n = rb_first(&obj_rule_pos_tree); n; n = rb_next(n)) {
		struct obj_rule *r = rb_container_of(n, struct obj_rule, pos_node);

		fr_printf(INFO, "rule % 6d % 6d  %d  ", r->chain_no, r->prio, r->state);
		struct tc_rule *tcr_h = r->have;

		if (tcr_h)
			fr_printf(INFO, "%-12s ", tc_rule_state_str(tcr_h->type));
		else
			fr_printf(INFO, "%-12s ", "");
		struct tc_rule *tcr_w = r->want;

		if (tcr_w)
			fr_printf(INFO, "%-12s ", tc_rule_state_str(tcr_w->type));
		else
			fr_printf(INFO, "%-12s ", "");
		if (tcr_h && tcr_w) {
			const size_t tcr_sz = sizeof(struct tc_rule);

			if (memcmp(tcr_h, tcr_w, tcr_sz) == 0) {
				fr_printf(INFO, "= matching");
			} else {
				size_t i = 1;

				while (i <= tcr_sz && memcmp(tcr_h, tcr_w, i) == 0)
					i++;
				fr_printf(INFO, "%zu bytes matching", i-1);
			}
		}
		fr_printf(INFO, " %d %d\n", r->obj.refcnt, r->obj.weak_refcnt);
		fr_printf(INFO, "\n");
		if (tcr_h && tcr_w && memcmp(tcr_h, tcr_w, sizeof(struct tc_rule)) != 0) {
			fr_printf(INFO, "vlan_id:\t% 4d % 4d\n", tcr_h->vlan_id, tcr_w->vlan_id);
			fr_printf(INFO, "flower_flags:\t%4x %4x\n", tcr_h->flower_flags, tcr_w->flower_flags);
			fr_printf(INFO, "traits:\t\t% 4d % 4d\n", tcr_h->traits, tcr_w->traits);
			fr_printf(INFO, "goto:\t\t% 4d % 4d\n", tcr_h->goto_target, tcr_w->goto_target);
			hexdumpf(stderr, tcr_h, sizeof(struct tc_rule));
			hexdumpf(stderr, tcr_w, sizeof(struct tc_rule));
			print_af_addr(&tcr_h->af_addr);
			print_af_addr(&tcr_w->af_addr);
			hexdumpf(stderr, &tcr_h->af_addr, sizeof(struct af_addr));
			hexdumpf(stderr, &tcr_w->af_addr, sizeof(struct af_addr));
		}
		if (tcr_w && !tcr_h)
			hexdumpf(stderr, tcr_w, sizeof(struct tc_rule));
		if (tcr_h && !tcr_w)
			hexdumpf(stderr, tcr_h, sizeof(struct tc_rule));
	}
	for (struct rb_node *n = rb_first(&obj_rule_laf_tree); n; n = rb_next(n)) {
		struct obj_rule *r = rb_container_of(n, struct obj_rule, laf_node);

		fr_printf(INFO, " ??? % 6d % 6d ", r->chain_no, r->prio);
		struct tc_rule *tcr_h = r->have;

		if (tcr_h)
			fr_printf(INFO, "%-12s", tc_rule_state_str(tcr_h->type));
		fr_printf(INFO, "\n");
		if (tcr_h)
			tc_rule_print(tcr_h);
	}
}

uint16_t obj_rule_find_available_prio(const uint32_t chain_no, const uint16_t min_prio)
{
	uint16_t ret = 0;

	for (struct rb_node *n = rb_first(&obj_rule_pos_tree); n; n = rb_next(n)) {
		struct obj_rule *r = rb_container_of(n, struct obj_rule, pos_node);

		if (r->chain_no < chain_no)
			continue;
		if (r->chain_no > chain_no)
			break;
		if (ret == 0 && r->prio > min_prio)
			break;
		if (r->prio == ret || ret < min_prio)
			ret = r->prio + 1;
		else if (ret >= min_prio)
			break;
	}
	if (ret < min_prio)
		ret = min_prio;
	AN(ret >= min_prio);
	return ret;
}

void obj_rule_clear_all(void)
{
	for (struct rb_node *n = rb_first(&obj_rule_laf_tree), *nn; n; n = nn) {
		nn = rb_next(n);
		struct obj_rule *r = rb_container_of(n, struct obj_rule, laf_node);

		obj_rule_ref(r);
		obj_set_state(rule, r, PRESENT);
		obj_rule_unref(r);
	}
	for (struct rb_node *n = rb_first(&obj_rule_pos_tree), *nn; n; n = nn) {
		nn = rb_next(n);
		struct obj_rule *r = rb_container_of(n, struct obj_rule, pos_node);

		obj_rule_ref(r);
		obj_set_state(rule, r, PRESENT);
		obj_rule_unref(r);
	}
}

void obj_rule_init(void)
{
	struct tc_action_callbacks *tacb = tc_action_get_callbacks();

	tacb->pre_install = obj_rule_pre_install;
	tacb->post_install = obj_rule_post_install;
}
