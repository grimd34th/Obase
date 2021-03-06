/* $OpenBSD: cmd-swap-pane.c,v 1.13 2011/05/08 21:12:52 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include "tmux.h"

/*
 * Swap two panes.
 */

void	cmd_swap_pane_key_binding(struct cmd *, int);
int	cmd_swap_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_swap_pane_entry = {
	"swap-pane", "swapp",
	"dDs:t:U", 0, 0,
	"[-dDU] " CMD_SRCDST_PANE_USAGE,
	0,
	cmd_swap_pane_key_binding,
	NULL,
	cmd_swap_pane_exec
};

void
cmd_swap_pane_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	if (key == '{')
		args_set(self->args, 'U', NULL);
	else if (key == '}')
		args_set(self->args, 'D', NULL);
}

int
cmd_swap_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct winlink		*src_wl, *dst_wl;
	struct window		*src_w, *dst_w;
	struct window_pane	*tmp_wp, *src_wp, *dst_wp;
	struct layout_cell	*src_lc, *dst_lc;
	u_int			 sx, sy, xoff, yoff;

	dst_wl = cmd_find_pane(ctx, args_get(args, 't'), NULL, &dst_wp);
	if (dst_wl == NULL)
		return (-1);
	dst_w = dst_wl->window;

	if (!args_has(args, 's')) {
		src_w = dst_w;
		if (args_has(self->args, 'D')) {
			src_wp = TAILQ_NEXT(dst_wp, entry);
			if (src_wp == NULL)
				src_wp = TAILQ_FIRST(&dst_w->panes);
		} else if (args_has(self->args, 'U')) {
			src_wp = TAILQ_PREV(dst_wp, window_panes, entry);
			if (src_wp == NULL)
				src_wp = TAILQ_LAST(&dst_w->panes, window_panes);
		} else
			return (0);
	} else {
		src_wl = cmd_find_pane(ctx, args_get(args, 's'), NULL, &src_wp);
		if (src_wl == NULL)
			return (-1);
		src_w = src_wl->window;
	}

	if (src_wp == dst_wp)
		return (0);

	tmp_wp = TAILQ_PREV(dst_wp, window_panes, entry);
	TAILQ_REMOVE(&dst_w->panes, dst_wp, entry);
	TAILQ_REPLACE(&src_w->panes, src_wp, dst_wp, entry);
	if (tmp_wp == src_wp)
		tmp_wp = dst_wp;
	if (tmp_wp == NULL)
		TAILQ_INSERT_HEAD(&dst_w->panes, src_wp, entry);
	else
		TAILQ_INSERT_AFTER(&dst_w->panes, tmp_wp, src_wp, entry);

	src_lc = src_wp->layout_cell;
	dst_lc = dst_wp->layout_cell;
	src_lc->wp = dst_wp;
	dst_wp->layout_cell = src_lc;
	dst_lc->wp = src_wp;
	src_wp->layout_cell = dst_lc;

	src_wp->window = dst_w;
	dst_wp->window = src_w;

	sx = src_wp->sx; sy = src_wp->sy;
	xoff = src_wp->xoff; yoff = src_wp->yoff;
	src_wp->xoff = dst_wp->xoff; src_wp->yoff = dst_wp->yoff;
	window_pane_resize(src_wp, dst_wp->sx, dst_wp->sy);
	dst_wp->xoff = xoff; dst_wp->yoff = yoff;
	window_pane_resize(dst_wp, sx, sy);

	if (!args_has(self->args, 'd')) {
		if (src_w != dst_w) {
			window_set_active_pane(src_w, dst_wp);
			window_set_active_pane(dst_w, src_wp);
		} else {
			tmp_wp = dst_wp;
			if (!window_pane_visible(tmp_wp))
				tmp_wp = src_wp;
			window_set_active_pane(src_w, tmp_wp);
		}
	} else {
		if (src_w->active == src_wp)
			window_set_active_pane(src_w, dst_wp);
		if (dst_w->active == dst_wp)
			window_set_active_pane(dst_w, src_wp);
	}
	if (src_w != dst_w) {
		if (src_w->last == src_wp)
			src_w->last = NULL;
		if (dst_w->last == dst_wp)
			dst_w->last = NULL;
	}
	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	return (0);
}
