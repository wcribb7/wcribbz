/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include "cli.h"
#include "cmd.h"
#include "progress.h"

#include "fs_path.h"
#include "futils.h"
#include "sighandler.h"

#define COMMAND_NAME "clone"

static int show_help;
static int quiet;

static git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

static char *remote_path, *local_path;
static bool local_path_exists;
static cli_progress progress = CLI_PROGRESS_INIT;

static const cli_opt_spec opts[] = {
	{ CLI_OPT_TYPE_SWITCH,   "help",          0, &show_help,  1,
	  CLI_OPT_USAGE_HIDDEN | CLI_OPT_USAGE_STOP_PARSING, NULL,
	  "display help about the " COMMAND_NAME " command" },

	{ CLI_OPT_TYPE_BOOL,      "quiet",      'q', &quiet,      0,
	  CLI_OPT_USAGE_DEFAULT,  NULL,         "do not display progress output" },
	{ CLI_OPT_TYPE_BOOL,      "bare",        0,  &clone_opts.bare, 1,
	  CLI_OPT_USAGE_DEFAULT,  NULL,         "don't create a working directory" },

	{ CLI_OPT_TYPE_LITERAL },
	{ CLI_OPT_TYPE_ARG,       "repository",  0, &remote_path, 0,
	  CLI_OPT_USAGE_REQUIRED, "repository", "path to repository to clone" },
	{ CLI_OPT_TYPE_ARG,       "directory",   0, &local_path,  0,
	  CLI_OPT_USAGE_DEFAULT,  "directory",  "directory to clone into" },
	{ 0 },
};

static void print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts);
	printf("\n");

	printf("Clone an existing repository into a local directory.\n");
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);
}

static char *compute_local_path(const char *orig_path)
{
	git_str local_path = GIT_STR_INIT;
	const char *slash;

	if ((slash = strrchr(orig_path, '/')) == NULL &&
	    (slash = strrchr(orig_path, '\\')) == NULL)
		git_str_puts(&local_path, orig_path);
	else
		git_str_puts(&local_path, slash + 1);

	if (clone_opts.bare) {
		if (!git_str_endswith(&local_path, ".git"))
			git_str_puts(&local_path, ".git");
	} else {
		if (git_str_endswith(&local_path, ".git"))
			git_str_shorten(&local_path, 4);
	}

	return git_str_detach(&local_path);
}

static bool validate_local_path(const char *path)
{
	if (!git_fs_path_exists(path))
		return false;

	if (!git_fs_path_isdir(path) || !git_fs_path_is_empty_dir(path))
		cli_die("fatal: destination path '%s' already exists and is not an empty directory.\n", path);

	return true;
}

static void cleanup(void)
{
	int rmdir_flags = GIT_RMDIR_REMOVE_FILES;

	cli_progress_abort(&progress);

	if (local_path_exists)
		rmdir_flags |= GIT_RMDIR_SKIP_ROOT;

	if (!git_fs_path_isdir(local_path))
		return;

	if (git_futils_rmdir_r(local_path, NULL, rmdir_flags) < 0)
		cli_die_git();
}

static void interrupt_cleanup(void)
{
	cleanup();
	exit(130);
}

int cmd_clone(int argc, char **argv)
{
	git_repository *repo = NULL;
	cli_opt invalid_opt;
	char *computed_path = NULL;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	if (show_help) {
		print_help();
		return 0;
	}

	if (!local_path)
		local_path = computed_path = compute_local_path(remote_path);

	local_path_exists = validate_local_path(local_path);

	git_sighandler_set_interrupt(interrupt_cleanup);

	if (!quiet) {
		clone_opts.fetch_opts.callbacks.sideband_progress = cli_progress_fetch_sideband;
		clone_opts.fetch_opts.callbacks.transfer_progress = cli_progress_fetch_transfer;
		clone_opts.fetch_opts.callbacks.payload = &progress;

		clone_opts.checkout_opts.progress_cb = cli_progress_checkout;
		clone_opts.checkout_opts.progress_payload = &progress;

		printf("Cloning into '%s'...\n", local_path);
	}

	if (git_clone(&repo, remote_path, local_path, &clone_opts) < 0) {
		cleanup();
		cli_die_git();
	}

	cli_progress_finish(&progress);

	cli_progress_dispose(&progress);
	git__free(computed_path);
	git_repository_free(repo);

	return 0;
}
