// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_queue.h"

/* TODO add timer to detect hung items, that never completes */

enum queue_item_state {
	QUEUE_ITEM_STATE_NEW,
	QUEUE_ITEM_STATE_SENT,
	QUEUE_ITEM_STATE_DONE,
};

struct queue_item {
	void (*execute)(EV_P_ void *data);
	void (*completed)(EV_P_ void *data, int nl_errno);
	void *data;
	struct queue_item *next;
	enum queue_item_state state;
};

struct queue {
	struct queue_item *head;
	struct queue_item *tail;
	bool is_busy;
	bool has_sent_request;
	struct conn *conn;
};

static struct queue Q = {NULL,};

static struct queue_item *queue_pop(void)
{
	struct queue_item *qi = Q.head;

	AN(qi);
	Q.head = qi->next;
	if (Q.tail == qi)
		Q.tail = NULL;
	return qi;
}

static void queue_process(EV_P)
{
	struct queue_item *qi;

	AN(!Q.is_busy);
	qi = Q.head;
	AN(qi->state == QUEUE_ITEM_STATE_NEW);
	AN(qi->execute);
	Q.is_busy = true;
	Q.has_sent_request = false;
	qi->state = QUEUE_ITEM_STATE_SENT;
	qi->execute(EV_A_ qi->data);
}

static void handle_queue_completed(EV_P_ struct conn *c, int nl_errno)
{
	struct queue_item *qi;

	AN(Q.conn == c);
	AN(Q.is_busy);
	qi = queue_pop();
	AN(qi->state == QUEUE_ITEM_STATE_SENT);
	qi->state = QUEUE_ITEM_STATE_DONE;
	Q.is_busy = false;
	if (qi->completed)
		qi->completed(EV_A_ qi->data, nl_errno);
	free(qi);
}

static void queue_process_loop(EV_P)
{
	while (Q.head != NULL && !Q.is_busy) {
		queue_process(EV_A);
		if (Q.has_sent_request)
			break;
		handle_queue_completed(EV_A_ Q.conn, 0);
	}
}

static void queue_is_complete(EV_P_ struct conn *c, int nl_errno)
{
	handle_queue_completed(EV_A_ c, nl_errno);
	if (Q.head != NULL && !Q.is_busy)
		queue_process_loop(EV_A);
}

static void queue_has_sent_request(struct conn *c)
{
	AN(Q.conn == c);
	Q.has_sent_request = true;
}

void queue_schedule(EV_P_ void (*execute)(EV_P_ void *data), void (*completed)(EV_P_ void *data, int nl_errno), void *data)
{
	struct queue_item *qi;

	AN(execute);
	qi = fr_malloc(sizeof(struct queue_item));
	qi->execute = execute;
	qi->completed = completed;
	qi->data = data;

	if (Q.tail != NULL)
		Q.tail->next = qi;
	else
		Q.head = qi;
	Q.tail = qi;
	if (!Q.is_busy)
		queue_process_loop(EV_A);
}

struct conn *queue_get_conn(void)
{
	AN(Q.conn);
	return Q.conn;
}

void queue_init(struct conn *c)
{
	AZ(Q.conn);
	memset(&Q, '\0', sizeof(Q));
	c->on_complete = queue_is_complete;
	c->on_send_req = queue_has_sent_request;
	Q.conn = c;
}

void queue_fini(void)
{
	AN(Q.conn);
	memset(&Q, '\0', sizeof(Q));
}

void nl_queue_status(void)
{
	fr_printf(DEBUG2, "queue status: %s\n", Q.is_busy ? "is_busy" : "ok");
}
