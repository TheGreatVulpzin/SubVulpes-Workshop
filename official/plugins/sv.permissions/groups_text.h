#ifndef SV_PERMISSION_GROUPS_TEXT_H
#define SV_PERMISSION_GROUPS_TEXT_H

#include "groups.h"
#include <stddef.h>

/* The groups as a text file that people can edit. Each group is a section with four keys:
 *   [moderator]
 *   level=50
 *   parent=member
 *   prefix=[Mod]
 *   nodes=core.kick,bans.ban,-admcmd.god
 * Blank lines and lines that start with # are skipped. */
#define PM_TEXT_MAX 32768u

/* Writes the groups. Gives the length of the text, or 0 when the buffer is too small. */
size_t pm_to_text(const PmGroups *groups, char *out, size_t capacity);

/* Reads the groups and checks them with pm_valid. 1 when the text is good. Otherwise 0, and error holds one line that says
 * what is wrong and where. The groups in out are not valid then. */
int pm_from_text(const char *text, PmGroups *out, char *error, size_t capacity);

#endif
