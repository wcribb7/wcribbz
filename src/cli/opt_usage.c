/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "cli.h"
#include "str.h"

/*
 * This is similar to adopt's function, but modified to understand
 * that we have a command ("git") and a "subcommand" ("checkout").
 * It also understands a terminal's line length and wrap appropriately,
 * using a `git_str` for storage.
 */
int cli_opt_usage_fprint(
	FILE *file,
	const char *command,
	const char *subcommand,
	const cli_opt_spec specs[])
{
	git_str usage = GIT_BUF_INIT, opt = GIT_BUF_INIT;
	const cli_opt_spec *spec;
	size_t i, prefixlen, linelen;
	bool choice = false;
	int error;

	/* TODO: query actual console width. */
	int console_width = 80;

	if ((error = git_str_printf(&usage, "usage: %s", command)) < 0)
		goto done;

	if (subcommand &&
	    (error = git_str_printf(&usage, " %s", subcommand)) < 0)
		goto done;

	linelen = git_str_len(&usage);
	prefixlen = linelen + 1;

	for (spec = specs; spec->type; ++spec) {
		int optional = !(spec->usage & CLI_OPT_USAGE_REQUIRED);
		bool next_choice = !!((spec+1)->usage & CLI_OPT_USAGE_CHOICE);

		if (spec->usage & CLI_OPT_USAGE_HIDDEN)
			continue;

		if (choice)
			git_str_putc(&opt, '|');
		else
			git_str_clear(&opt);

		if (optional && !choice)
			git_str_putc(&opt, '[');
		if (!optional && !choice && next_choice)
			git_str_putc(&opt, '(');

		if (spec->type == CLI_OPT_VALUE && spec->alias)
			error = git_str_printf(&opt, "-%c <%s>", spec->alias, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE)
			error = git_str_printf(&opt, "--%s=<%s>", spec->name, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE_OPTIONAL && spec->alias)
			error = git_str_printf(&opt, "-%c [<%s>]", spec->alias, spec->value_name);
		else if (spec->type == CLI_OPT_VALUE_OPTIONAL)
			error = git_str_printf(&opt, "--%s[=<%s>]", spec->name, spec->value_name);
		else if (spec->type == CLI_OPT_ARG)
			error = git_str_printf(&opt, "<%s>", spec->value_name);
		else if (spec->type == CLI_OPT_ARGS)
			error = git_str_printf(&opt, "<%s...>", spec->value_name);
		else if (spec->type == CLI_OPT_LITERAL)
			error = git_str_printf(&opt, "--");
		else if (spec->alias && !(spec->usage & CLI_OPT_USAGE_SHOW_LONG))
			error = git_str_printf(&opt, "-%c", spec->alias);
		else
			error = git_str_printf(&opt, "--%s", spec->name);

		if (error < 0)
			goto done;

		if (!optional && choice && !next_choice)
			git_str_putc(&opt, ')');
		else if (optional && !next_choice)
			git_str_putc(&opt, ']');

		if ((choice = next_choice))
			continue;

		if (git_str_oom(&opt)) {
			error = -1;
			goto done;
		}

		if (linelen > prefixlen &&
		    console_width > 0 &&
		    linelen + git_str_len(&opt) + 1 > (size_t)console_width) {
			git_str_putc(&usage, '\n');

			for (i = 0; i < prefixlen; i++)
				git_str_putc(&usage, ' ');

			linelen = prefixlen;
		} else {
			git_str_putc(&usage, ' ');
			linelen += git_str_len(&opt) + 1;
		}

		git_str_puts(&usage, git_str_cstr(&opt));

		if (git_str_oom(&usage)) {
			error = -1;
			goto done;
		}
	}

	error = fprintf(file, "%s\n", git_str_cstr(&usage));

done:
	error = (error < 0) ? -1 : 0;

	git_str_dispose(&usage);
	git_str_dispose(&opt);
	return error;
}

