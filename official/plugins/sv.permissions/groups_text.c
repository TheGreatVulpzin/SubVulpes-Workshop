#include "groups_text.h"
#include <stdio.h>
#include <string.h>

size_t pm_to_text(const PmGroups *groups, char *out, size_t capacity)
{
    size_t used = 0;
    uint32_t i, n;
    int written;
    written = snprintf(out, capacity, "# Permission groups. Edit this file, then type /group reload in the game.\n");
    if (written < 0 || (size_t)written >= capacity) return 0;
    used = (size_t)written;
    for (i = 0; i < groups->count; ++i) {
        const PmGroup *g = &groups->group[i];
        written = snprintf(out + used, capacity - used, "\n[%s]\nlevel=%d\nparent=%s\nprefix=%s\nnodes=", g->name, g->level,
            g->parent, g->prefix);
        if (written < 0 || (size_t)written >= capacity - used) return 0;
        used += (size_t)written;
        for (n = 0; n < g->node_count; ++n) {
            written = snprintf(out + used, capacity - used, "%s%s", n ? "," : "", g->nodes[n]);
            if (written < 0 || (size_t)written >= capacity - used) return 0;
            used += (size_t)written;
        }
        if (used + 2 > capacity) return 0;
        out[used++] = '\n';
        out[used] = 0;
    }
    return used;
}

static void fail(char *error, size_t capacity, unsigned line, const char *reason)
{
    snprintf(error, capacity, "line %u: %s", line, reason);
}

static char *trim(char *text)
{
    char *end;
    while (*text == ' ' || *text == '\t') ++text;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) *--end = 0;
    return text;
}

static int read_level(const char *text, int32_t *out)
{
    int32_t value = 0;
    size_t i;
    if (!*text || strlen(text) > 3) return 0;
    for (i = 0; text[i]; ++i) {
        if (text[i] < '0' || text[i] > '9') return 0;
        value = value * 10 + (text[i] - '0');
    }
    if (value > 100) return 0;
    *out = value;
    return 1;
}

static int read_nodes(char *list, PmGroup *g, char *error, size_t capacity, unsigned line)
{
    g->node_count = 0;
    while (*list) {
        char *end = strchr(list, ','), *node;
        if (end) *end = 0;
        node = trim(list);
        if (*node) {
            if (g->node_count >= PM_NODE_MAX) { fail(error, capacity, line, "a group holds at most 20 nodes"); return 0; }
            if (!pm_node(node, 1)) { fail(error, capacity, line, "a node is not valid"); return 0; }
            strcpy(g->nodes[g->node_count++], node);
        }
        if (!end) break;
        list = end + 1;
    }
    return 1;
}

static int set_key(PmGroup *g, const char *key, char *value, char *error, size_t capacity, unsigned line)
{
    if (!strcmp(key, "level")) {
        if (!read_level(value, &g->level)) { fail(error, capacity, line, "level must be a number from 0 to 100"); return 0; }
    } else if (!strcmp(key, "parent")) {
        if (strlen(value) >= sizeof g->parent) { fail(error, capacity, line, "the parent name is too long"); return 0; }
        strcpy(g->parent, value);
    } else if (!strcmp(key, "prefix")) {
        if (strlen(value) >= sizeof g->prefix) { fail(error, capacity, line, "a prefix holds at most 31 bytes"); return 0; }
        strcpy(g->prefix, value);
    } else if (!strcmp(key, "nodes")) {
        return read_nodes(value, g, error, capacity, line);
    } else {
        fail(error, capacity, line, "unknown key (use level, parent, prefix or nodes)");
        return 0;
    }
    return 1;
}

int pm_from_text(const char *text, PmGroups *out, char *error, size_t capacity)
{
    char line[256];
    unsigned number = 0;
    PmGroup *current = NULL;
    memset(out, 0, sizeof *out);
    error[0] = 0;
    while (*text) {
        size_t length = strcspn(text, "\n");
        char *body, *equals;
        ++number;
        if (length >= sizeof line) { fail(error, capacity, number, "the line is too long"); return 0; }
        memcpy(line, text, length);
        line[length] = 0;
        text += length + (text[length] == '\n');
        body = trim(line);
        if (!*body || *body == '#') continue;
        if (*body == '[') {
            size_t n = strlen(body);
            if (body[n - 1] != ']') { fail(error, capacity, number, "a section header must end with ]"); return 0; }
            body[n - 1] = 0;
            ++body;
            if (!pm_name(body)) { fail(error, capacity, number, "the group name is not valid"); return 0; }
            if (pm_find(out, body) >= 0) { fail(error, capacity, number, "the group appears twice"); return 0; }
            if (out->count >= SV_PERMISSION_GROUP_MAX) { fail(error, capacity, number, "too many groups"); return 0; }
            current = &out->group[out->count++];
            memset(current, 0, sizeof *current);
            current->format = 1;
            strcpy(current->name, body);
            continue;
        }
        equals = strchr(body, '=');
        if (!equals) { fail(error, capacity, number, "expected key=value"); return 0; }
        if (!current) { fail(error, capacity, number, "a key must come after a [group] line"); return 0; }
        *equals = 0;
        if (!set_key(current, trim(body), trim(equals + 1), error, capacity, number)) return 0;
    }
    if (!pm_valid(out)) {
        snprintf(error, capacity, "the groups are not valid: keep the five default groups, keep owner at level 100, and use only parents that exist");
        return 0;
    }
    return 1;
}
