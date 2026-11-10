#include "../../common/permissions.h"
#include "../sv.patchouli/sv_patchouli.h"
#include "../sv.ayalog/sv_ayalog.h"
#include "bans_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Bans and kicks. The ban table lives in plugin storage (server scope), one value for each slot. The plugin refuses a
 * banned account, and optionally a banned address, at the join. */

static Official api;
static BanTable table;
static int loaded;

enum { PAGE = 5 };

static uint32_t clock_now(void)
{
    return (uint32_t)time(NULL);
}

static void slot_key(int slot, char *key, size_t capacity)
{
    snprintf(key, capacity, "ban%d", slot);
}

static void clean(Ban *ban)
{
    ban->by[BAN_NAME - 1] = 0;
    ban->name[BAN_NAME - 1] = 0;
    ban->reason[BAN_REASON - 1] = 0;
    ban->used = ban->used ? 1 : 0;
}

static SvResult load_table(void)
{
    int i;
    if (loaded) return SV_OK;
    memset(&table, 0, sizeof table);
    for (i = 0; i < BAN_MAX; ++i) {
        char key[16];
        Ban ban;
        SvResult result;
        slot_key(i, key, sizeof key);
        result = of_get(&api, SV_STORAGE_SERVER, 0, key, &ban, sizeof ban);
        if (result == SV_OK) { clean(&ban); table.ban[i] = ban; }
        else if (result != SV_NOT_FOUND) return result;
    }
    loaded = 1;
    return SV_OK;
}

static SvResult save_slot(int slot)
{
    char key[16];
    slot_key(slot, key, sizeof key);
    if (table.ban[slot].used) return of_put(&api, SV_STORAGE_SERVER, 0, key, &table.ban[slot], sizeof table.ban[slot]);
    return of_erase(&api, SV_STORAGE_SERVER, 0, key);
}

static void record(const char *action, SvEntity actor, SvEntity target, SvResult result, const char *detail)
{
    const void *service_table = NULL;
    const SvLogging *service;
    SvLogEntry entry = { sizeof entry };
    if (api.plugins->borrow(api.owner, SV_LOGGING_API, SV_LOGGING_VERSION, &service_table) != SV_OK || !service_table) return;
    service = (const SvLogging *)service_table;
    if (service->size < sizeof *service || service->version != SV_LOGGING_VERSION || !service->record) return;
    entry.kind = result == SV_OK ? SV_LOG_ACTION : SV_LOG_FAILURE;
    entry.action = action; entry.actor = actor; entry.target = target; entry.result = result; entry.detail = detail;
    service->record(api.owner, &entry);
}

static void actor_name(SvEntity actor, char *name, size_t capacity)
{
    SvPlayerState player = { sizeof player };
    if (actor && api.read->player(api.owner, actor, &player) == SV_OK) snprintf(name, capacity, "%s", player.name);
    else snprintf(name, capacity, "console");
}

static void announce(const char *format, const char *name, const char *reason)
{
    char text[200];
    if (reason && *reason) snprintf(text, sizeof text, format, name, reason);
    else snprintf(text, sizeof text, format, name, "no reason given");
    api.write->message(api.owner, SV_MESSAGE_SERVER, text);
}

/* A player may not be banned or kicked by a command when the group has the node bans.immune, or when it is a bot. */
static int protected_player(SvEntity actor, const SvPlayerState *target)
{
    if (target->is_bot) { of_tell(&api, actor, "%s is a bot. Use /delh or /kill.", target->name); return 1; }
    if (actor && target->entity == actor) { of_tell(&api, actor, "You cannot do this to yourself."); return 1; }
    if (actor && of_can(&api, target->entity, "bans.immune")) {
        of_tell(&api, actor, "%s cannot be banned or kicked.", target->name);
        return 1;
    }
    return 0;
}

/* Ends the connection and keeps the address off the core kick list, so the player can join again. */
static SvResult disconnect(const SvPlayerState *target)
{
    SvConnectionState connection = { sizeof connection };
    SvResult found = api.read->connection(api.owner, target->entity, &connection);
    SvResult result = api.write->kick(api.owner, target->entity);
    if (found == SV_OK) api.write->kicked_remove(api.owner, connection.ip);
    return result;
}

static const char *rest_of(char *rest)
{
    while (*rest == ' ') ++rest;
    return rest;
}

static void cut_text(char *text, size_t capacity)
{
    size_t i;
    if (strlen(text) >= capacity) text[capacity - 1] = 0;
    for (i = 0; text[i]; ++i)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) text[i] = '?';
}

static SvResult do_kick(SvEntity actor, char *arguments)
{
    char name[64], reason[BAN_REASON], by[BAN_NAME];
    char *rest = arguments;
    SvPlayerState target = { sizeof target };
    SvResult result;
    if (of_token(&rest, name, sizeof name) != 1) return SV_INVALID;
    snprintf(reason, sizeof reason, "%s", rest_of(rest));
    cut_text(reason, sizeof reason);
    result = of_target(&api, actor, name, &target);
    if (result != SV_OK) return result;
    if (protected_player(actor, &target)) return SV_DENIED;
    result = disconnect(&target);
    if (result != SV_OK) return result;
    actor_name(actor, by, sizeof by);
    announce("%s was kicked: %s.", target.name, reason);
    of_tell(&api, actor, "Kicked %s. The player can join again.", target.name);
    record("kick", actor, target.entity, SV_OK, reason[0] ? reason : NULL);
    return SV_OK;
}

static int32_t default_days(void)
{
    int32_t days = of_int(&api, "default_days", 0);
    return days < 0 ? 0 : days > BAN_DAYS_MAX ? BAN_DAYS_MAX : days;
}

static SvResult do_ban(SvEntity actor, char *arguments)
{
    char name[64], word[32];
    char *rest = arguments, *after;
    SvPlayerState target = { sizeof target };
    SvConnectionState connection = { sizeof connection };
    Ban ban;
    uint32_t seconds = (uint32_t)default_days() * 86400u, now = clock_now();
    int slot;
    SvResult result;
    if (of_token(&rest, name, sizeof name) != 1) return SV_INVALID;
    after = rest;
    if (of_token(&after, word, sizeof word) == 1 && ban_duration(word, &seconds)) rest = after;
    memset(&ban, 0, sizeof ban);
    snprintf(ban.reason, sizeof ban.reason, "%s", rest_of(rest));
    cut_text(ban.reason, sizeof ban.reason);
    result = of_target(&api, actor, name, &target);
    if (result != SV_OK) return result;
    if (protected_player(actor, &target)) return SV_DENIED;
    ban.account = of_account(&api, &target);
    if (of_int(&api, "ban_address", 1) && api.read->connection(api.owner, target.entity, &connection) == SV_OK) {
        ban.has_address = 1;
        memcpy(ban.address, connection.ip, 4);
    }
    if (ban.account < 0 && !ban.has_address) {
        of_tell(&api, actor, "%s has no account, and address bans are off. Nothing to ban.", target.name);
        return SV_DENIED;
    }
    snprintf(ban.name, sizeof ban.name, "%s", target.name);
    actor_name(actor, ban.by, sizeof ban.by);
    ban.created = now;
    ban.until = seconds ? now + seconds : 0;
    slot = ban_add(&table, now, &ban);
    if (slot < 0) { of_tell(&api, actor, "The ban list is full (%d). Remove a ban with /unban.", BAN_MAX); return SV_LIMIT; }
    result = save_slot(slot);
    if (result != SV_OK) {
        ban_remove(&table, slot);
        of_tell(&api, actor, "The ban could not be saved. Check the plugin storage.");
        return result;
    }
    disconnect(&target);
    {
        char left[16], detail[80];
        ban_left(&ban, now, left, sizeof left);
        announce("%s was banned: %s.", target.name, ban.reason);
        of_tell(&api, actor, "Banned %s (ban %d, %s).", target.name, slot, left);
        snprintf(detail, sizeof detail, "ban %d %s %s", slot, left, ban.reason);
        record("ban", actor, target.entity, SV_OK, detail);
    }
    return SV_OK;
}

static SvResult do_unban(SvEntity actor, char *arguments)
{
    char word[16];
    char *rest = arguments;
    int32_t slot;
    char detail[40];
    if (of_token(&rest, word, sizeof word) != 1 || *rest) return SV_INVALID;
    if (!of_number(word, 0, BAN_MAX - 1, &slot) || !ban_active(&table.ban[slot], clock_now())) {
        of_tell(&api, actor, "No active ban has the number %.10s. See /bans.", word);
        return SV_NOT_FOUND;
    }
    snprintf(detail, sizeof detail, "ban %d %s", (int)slot, table.ban[slot].name);
    ban_remove(&table, (int)slot);
    if (save_slot((int)slot) != SV_OK) of_tell(&api, actor, "The ban left the list, but the storage did not change.");
    of_tell(&api, actor, "Ban %d is removed.", (int)slot);
    record("unban", actor, 0, SV_OK, detail);
    return SV_OK;
}

static SvResult do_bans(SvEntity actor, char *arguments)
{
    char word[16];
    char *rest = arguments;
    int32_t page = 1;
    int i, shown = 0, seen = 0, active = 0, first;
    uint32_t now = clock_now();
    if (of_token(&rest, word, sizeof word) == 1 && (*rest || !of_number(word, 1, 999, &page))) return SV_INVALID;
    for (i = 0; i < BAN_MAX; ++i) if (ban_active(&table.ban[i], now)) ++active;
    if (!active) { of_tell(&api, actor, "No active bans."); return SV_OK; }
    first = (page - 1) * PAGE;
    of_tell(&api, actor, "Bans: %d. Page %d of %d.", active, (int)page, (active + PAGE - 1) / PAGE);
    for (i = 0; i < BAN_MAX && shown < PAGE; ++i) {
        char left[16];
        const Ban *ban = &table.ban[i];
        if (!ban_active(ban, now)) continue;
        if (seen++ < first) continue;
        ban_left(ban, now, left, sizeof left);
        of_tell(&api, actor, "%d %s %s %s", i, ban->name, left, ban->reason);
        ++shown;
    }
    if (!shown) of_tell(&api, actor, "No bans on this page.");
    return SV_OK;
}

static SvResult do_baninfo(SvEntity actor, char *arguments)
{
    char word[16], left[16];
    char *rest = arguments;
    int32_t slot;
    uint32_t now = clock_now();
    const Ban *ban;
    if (of_token(&rest, word, sizeof word) != 1 || *rest) return SV_INVALID;
    if (!of_number(word, 0, BAN_MAX - 1, &slot) || !ban_active(&table.ban[slot], now)) {
        of_tell(&api, actor, "No active ban has the number %.10s. See /bans.", word);
        return SV_NOT_FOUND;
    }
    ban = &table.ban[slot];
    ban_left(ban, now, left, sizeof left);
    of_tell(&api, actor, "Ban %d: %s, account %d. Left: %s.", (int)slot, ban->name, (int)ban->account, left);
    of_tell(&api, actor, "By %s, %u minutes ago. Reason: %s", ban->by, (now - ban->created) / 60u,
        ban->reason[0] ? ban->reason : "none");
    if (ban->has_address) of_tell(&api, actor, "Address: %u.%u.x.x", ban->address[0], ban->address[1]);
    return SV_OK;
}

typedef SvResult (*Handler)(SvEntity actor, char *arguments);

static SvResult run(const char *name, const char *node, Handler handler, SvEntity actor, const char *arguments)
{
    char line[160];
    SvResult result = load_table();
    if (!of_can(&api, actor, node)) { of_tell(&api, actor, "You need the permission %s for this.", node); return SV_OK; }
    if (result != SV_OK) { of_tell(&api, actor, "Cannot read the ban list. Check the plugin storage."); return SV_OK; }
    if (strnlen(arguments, sizeof line) >= sizeof line) { of_tell(&api, actor, "Command text is too long."); return SV_OK; }
    strcpy(line, arguments);
    result = handler(actor, line);
    if (result == SV_INVALID) of_tell(&api, actor, "Wrong arguments. See /help %s.", name);
    if (result != SV_OK && result != SV_INVALID && result != SV_DENIED && result != SV_NOT_FOUND && result != SV_LIMIT)
        record(name, actor, 0, result, NULL);
    return SV_OK;
}

#define COMMAND(name, node, handler) \
    static SvResult SV_CALL cmd_##name(SvOwner owner, uint64_t actor, const char *args) \
    { (void)owner; return run(#name, node, handler, actor, args); }
COMMAND(kick, "bans.kick", do_kick)
COMMAND(ban, "bans.ban", do_ban)
COMMAND(unban, "bans.unban", do_unban)
COMMAND(bans, "bans.view", do_bans)
COMMAND(baninfo, "bans.view", do_baninfo)
#undef COMMAND

static const struct { const char *command, *usage, *summary, *node; } topic_list[] = {
    { "kick", "PLAYER [REASON]", "Disconnect a player. The player can join again", "bans.kick" },
    { "ban", "PLAYER [TIME] [REASON]", "Ban the account. TIME: 30m, 12h, 7d, 2w or perm", "bans.ban" },
    { "unban", "N", "End ban N from the list", "bans.unban" },
    { "bans", "[PAGE]", "List the active bans", "bans.view" },
    { "baninfo", "N", "Show one ban: who, by whom, time left, reason", "bans.view" }
};

static int register_topics(SvOwner owner)
{
    const void *service_table = NULL;
    const SvPatchouli *service;
    SvPatchouliTopic topic = { sizeof topic };
    uint32_t i;
    if (api.plugins->borrow(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &service_table) != SV_OK || !service_table) return 0;
    service = (const SvPatchouli *)service_table;
    if (service->size < sizeof *service || service->version != SV_PATCHOULI_VERSION || !service->topic) return 0;
    for (i = 0; i < sizeof topic_list / sizeof topic_list[0]; ++i) {
        topic.command = topic_list[i].command; topic.usage = topic_list[i].usage;
        topic.summary = topic_list[i].summary; topic.node = topic_list[i].node;
        if (service->topic(owner, &topic) != SV_OK) return 0;
    }
    return 1;
}

static void forget_topics(SvOwner owner)
{
    const void *service_table = NULL;
    if (api.plugins && api.plugins->size >= offsetof(SvPlugins, borrow) + sizeof api.plugins->borrow &&
        api.plugins->borrow(owner, SV_PATCHOULI_API, SV_PATCHOULI_VERSION, &service_table) == SV_OK && service_table &&
        ((const SvPatchouli *)service_table)->size >= sizeof(SvPatchouli))
        ((const SvPatchouli *)service_table)->forget(owner);
}

/* The join decision. A banned account, or a banned address when the table holds one, is refused. */
static SvResult SV_CALL event(SvOwner owner, const SvEvent *base)
{
    const SvGameEvent *join = (const SvGameEvent *)base;
    SvPlayerState player = { sizeof player };
    SvConnectionState connection = { sizeof connection };
    const uint8_t *address = NULL;
    uint32_t now = clock_now();
    int slot;
    (void)owner;
    if (base->kind != SV_EVENT_JOIN || base->size < sizeof(SvGameEvent)) return SV_OK;
    if (load_table() != SV_OK) return SV_OK;
    if (api.read->player(api.owner, join->player, &player) != SV_OK || player.is_bot) return SV_OK;
    if (api.read->connection(api.owner, join->player, &connection) == SV_OK) address = connection.ip;
    slot = ban_find(&table, now, of_account(&api, &player), address);
    if (slot < 0) return SV_OK;
    of_tell(&api, 0, "Refused %s: ban %d.", player.name, slot);
    return SV_DENIED;
}

static SvResult SV_CALL enable(const SvHost *host, SvOwner owner)
{
    static const struct { const char *name; SvCommandFn callback; } commands[] = {
        { "kick", cmd_kick }, { "ban", cmd_ban }, { "unban", cmd_unban }, { "bans", cmd_bans }, { "baninfo", cmd_baninfo }
    };
    uint32_t i;
    SvResult result = of_init(&api, host, owner, OF_CAP(SV_API_GAME_READ) | OF_CAP(SV_API_GAME_WRITE) |
        OF_CAP(SV_API_STORAGE) | OF_CAP(SV_API_CONFIG) | OF_CAP(SV_API_PLUGINS));
    if (result != SV_OK) return result;
    if (api.plugins->size < offsetof(SvPlugins, borrow) + sizeof api.plugins->borrow) return SV_INCOMPATIBLE;
    memset(&table, 0, sizeof table); loaded = 0;
    register_topics(owner);
    for (i = 0; result == SV_OK && i < sizeof commands / sizeof commands[0]; ++i)
        result = host->command(owner, commands[i].name, SV_COMMAND_PUBLIC, commands[i].callback);
    if (result == SV_OK) result = host->subscribe(owner, SV_EVENT_JOIN, event);
    return result;
}

static void SV_CALL disable(SvOwner owner)
{
    forget_topics(owner);
    memset(&table, 0, sizeof table); loaded = 0;
}

SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin)
{
    if (abi != SV_PLUGIN_ABI || !plugin || plugin->size < sizeof *plugin) return SV_INCOMPATIBLE;
    memset(plugin, 0, sizeof *plugin);
    plugin->size = sizeof *plugin; plugin->abi = SV_PLUGIN_ABI;
    strcpy(plugin->id, SV_OFFICIAL_ID); strcpy(plugin->version, SV_OFFICIAL_VERSION);
    plugin->flags = SV_PLUGIN_LIVE; plugin->enable = enable; plugin->disable = disable;
    return SV_OK;
}
