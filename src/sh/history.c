#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

#define SH_HISTORY_MAX 64

static char *sh_history[SH_HISTORY_MAX];
static size_t sh_history_count;
static size_t sh_history_index;
static bool sh_history_active;
static char *sh_history_saved;

static void sh_history_clear_saved(void)
{
	free(sh_history_saved);
	sh_history_saved = NULL;
}

void sh_history_add(const char *line)
{
	if (!line || line[0] == '\0')
		return;
	if (sh_history_count > 0 &&
		strcmp(sh_history[sh_history_count - 1], line) == 0)
		return;
	if (sh_history_count == SH_HISTORY_MAX) {
		free(sh_history[0]);
		memmove(sh_history, sh_history + 1,
				(SH_HISTORY_MAX - 1) * sizeof(*sh_history));
		sh_history_count--;
	}
	sh_history[sh_history_count] = strdup(line);
	if (sh_history[sh_history_count])
		sh_history_count++;
	sh_history_active = false;
}

const char *sh_history_prev(const char *current)
{
	if (sh_history_count == 0)
		return NULL;
	if (!sh_history_active) {
		sh_history_clear_saved();
		sh_history_saved = strdup(current ? current : "");
		sh_history_active = true;
		sh_history_index = sh_history_count - 1;
	} else if (sh_history_index > 0) {
		sh_history_index--;
	}
	return sh_history[sh_history_index];
}

const char *sh_history_next(void)
{
	if (!sh_history_active)
		return NULL;
	if (sh_history_index + 1 < sh_history_count) {
		sh_history_index++;
		return sh_history[sh_history_index];
	}
	sh_history_active = false;
	return sh_history_saved ? sh_history_saved : "";
}
