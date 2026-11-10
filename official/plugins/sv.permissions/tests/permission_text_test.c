#include "../groups_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static char text[PM_TEXT_MAX];
static char error[200];
static PmGroups defaults, parsed;

static void round_trip(void)
{
    size_t length;
    uint32_t i, n;
    pm_defaults(&defaults);
    length = pm_to_text(&defaults, text, sizeof text);
    CHECK(length > 0 && length < sizeof text);
    CHECK(pm_from_text(text, &parsed, error, sizeof error));
    CHECK(parsed.count == defaults.count);
    for (i = 0; i < defaults.count; ++i) {
        const PmGroup *a = &defaults.group[i], *b = &parsed.group[i];
        CHECK(!strcmp(a->name, b->name) && !strcmp(a->parent, b->parent) && !strcmp(a->prefix, b->prefix));
        CHECK(a->level == b->level && a->node_count == b->node_count);
        for (n = 0; n < a->node_count; ++n) CHECK(!strcmp(a->nodes[n], b->nodes[n]));
    }
    CHECK(pm_to_text(&defaults, text, 40) == 0);
}

static const char *edit(const char *from, const char *to)
{
    static char copy[PM_TEXT_MAX + 256];
    char *at;
    pm_to_text(&defaults, copy, sizeof copy);
    at = strstr(copy, from);
    CHECK(at != NULL);
    memmove(at + strlen(to), at + strlen(from), strlen(at + strlen(from)) + 1);
    memcpy(at, to, strlen(to));
    return copy;
}

static void accepts(void)
{
    /* New group, comments, CRLF lines, spaces around keys and nodes, and a negative node. */
    const char *good = "# comment\r\n[guest]\r\nlevel = 0\r\nparent=\r\nprefix=\r\nnodes= permissions.view , chatadd.msg\r\n"
        "[member]\nlevel=10\nparent=guest\nprefix=[Member]\nnodes=\n"
        "[moderator]\nlevel=50\nparent=member\nprefix=[Mod]\nnodes=core.kick,-admcmd.god\n"
        "[admin]\nlevel=90\nparent=moderator\nprefix=[Admin]\nnodes=core.*\n"
        "[owner]\nlevel=100\nparent=admin\nprefix=[Owner]\nnodes=*\n"
        "[vip]\nlevel=20\nparent=member\nprefix=[VIP =)]\nnodes=admcmd.heal\n";
    CHECK(pm_from_text(good, &parsed, error, sizeof error));
    CHECK(parsed.count == 6);
    CHECK(!strcmp(parsed.group[0].nodes[1], "chatadd.msg") && parsed.group[0].node_count == 2);
    CHECK(!strcmp(parsed.group[5].prefix, "[VIP =)]"));
    CHECK(!strcmp(parsed.group[2].nodes[1], "-admcmd.god"));
}

static void refuses(void)
{
    CHECK(!pm_from_text("", &parsed, error, sizeof error));
    CHECK(!pm_from_text(edit("level=50", "level=500"), &parsed, error, sizeof error) && strstr(error, "level"));
    CHECK(!pm_from_text(edit("level=50", "level=abc"), &parsed, error, sizeof error));
    CHECK(!pm_from_text(edit("level=50", "colour=50"), &parsed, error, sizeof error) && strstr(error, "unknown key"));
    CHECK(!pm_from_text(edit("parent=member", "parent=nobody"), &parsed, error, sizeof error) && strstr(error, "not valid"));
    CHECK(!pm_from_text(edit("[member]", "[guest]"), &parsed, error, sizeof error) && strstr(error, "twice"));
    CHECK(!pm_from_text(edit("[member]", "[Member One]"), &parsed, error, sizeof error) && strstr(error, "name"));
    CHECK(!pm_from_text(edit("[member]", "[member"), &parsed, error, sizeof error) && strstr(error, "]"));
    CHECK(!pm_from_text(edit("core.kick", "Core.Kick"), &parsed, error, sizeof error) && strstr(error, "node"));
    CHECK(!pm_from_text(edit("level=100", "level=99"), &parsed, error, sizeof error));
    CHECK(!pm_from_text("level=5\n", &parsed, error, sizeof error) && strstr(error, "line 1"));
    CHECK(!pm_from_text("[guest]\nlevel\n", &parsed, error, sizeof error) && strstr(error, "line 2"));
}

static void limits(void)
{
    char big[PM_TEXT_MAX];
    int i;
    size_t used = 0;
    pm_defaults(&defaults);
    pm_to_text(&defaults, big, sizeof big);
    used = strlen(big);
    for (i = 0; i < 40; ++i) used += (size_t)sprintf(big + used, "[g%d]\nlevel=1\nparent=guest\nprefix=\nnodes=\n", i);
    CHECK(!pm_from_text(big, &parsed, error, sizeof error) && strstr(error, "too many"));
    {
        char many[1024] = "[guest]\nnodes=";
        for (i = 0; i < 21; ++i) strcat(many, i ? ",a.b" : "a.b");
        CHECK(!pm_from_text(many, &parsed, error, sizeof error) && strstr(error, "20"));
    }
    {
        char longline[400];
        memset(longline, 'x', sizeof longline - 1);
        longline[sizeof longline - 1] = 0;
        CHECK(!pm_from_text(longline, &parsed, error, sizeof error) && strstr(error, "too long"));
    }
}

int main(void)
{
    round_trip();
    accepts();
    refuses();
    limits();
    puts("permission text: all checks passed");
    return 0;
}
