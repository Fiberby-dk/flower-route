// SPDX-License-Identifier: GPL-2.0-or-later

#include "tc_action.h"
#include "nl_queue.h"
#include "tc_encode.h"
#include "nl_send.h"

struct tc_action {
	uint32_t chain_no;
	uint16_t prio;
	struct tc_rule *tcr;
	void *data;
};

static void tc_action_do_install(EV_P_ const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr)
{
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct conn *c = queue_get_conn();
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	tc_encode_rule(nlh, chain_no, prio, tcr, NO_TCE_FLAGS);
	AZ(config->dry_run);
	nl_send_req(EV_A_ c, nlh);
}

static void tc_action_do_install_dry_run(EV_P_ const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr)
{
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct conn *c = queue_get_conn();
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	tc_encode_rule(nlh, chain_no, prio, tcr, NO_TCE_FLAGS);
	AZ(config->dry_run);
	nl_send_req(EV_A_ c, nlh);
}

static struct tc_action_callbacks tacb = {
	.install = tc_action_do_install,
};

struct tc_action_callbacks *tc_action_get_callbacks(void)
{
	return &tacb;
}

static void tc_action_execute(EV_P_ void *data)
{
	struct tc_action *tca = data;

	AN(tacb.install);
	if (tacb.pre_install)
		tacb.pre_install(tca->data);
	tacb.install(EV_A_ tca->chain_no, tca->prio, tca->tcr);
	if (tacb.post_install)
		tacb.post_install(tca->data);
}

static void tc_action_done(EV_P_ void *data, const int nl_errno)
{
	struct tc_action *tca = data;

	if (tacb.done)
		tacb.done(tca->data, nl_errno);
	free(tca);
}

void tc_action_install(const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr, void *data)
{
	struct ev_loop *loop = EV_DEFAULT; /* TODO find a better way */
	struct tc_action *tca = fr_malloc(sizeof(struct tc_action));

	tca->chain_no = chain_no;
	tca->prio = prio;
	tca->tcr = tcr;
	tca->data = data;

	queue_schedule(EV_A_ tc_action_execute, tc_action_done, tca);
}
