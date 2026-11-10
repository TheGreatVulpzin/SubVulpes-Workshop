#include "bans_table.h"
#include <stdio.h>
#include <string.h>

int ban_duration(const char *text, uint32_t *seconds)
{
    uint64_t value = 0, unit;
    size_t i = 0;
    if (!text || !seconds) return 0;
    if (!strcmp(text, "perm") || !strcmp(text, "forever")) { *seconds = 0; return 1; }
    while (text[i] >= '0' && text[i] <= '9') {
        value = value * 10 + (uint64_t)(text[i] - '0');
        if (value > 100000) return 0;
        ++i;
    }
    if (!i || !value || !text[i] || text[i + 1]) return 0;
    switch (text[i]) {
    case 'm': unit = 60; break;
    case 'h': unit = 3600; break;
    case 'd': unit = 86400; break;
    case 'w': unit = 604800; break;
    default: return 0;
    }
    if (value * unit > (uint64_t)BAN_DAYS_MAX * 86400) return 0;
    *seconds = (uint32_t)(value * unit);
    return 1;
}

int ban_active(const Ban *ban, uint32_t now)
{
    return ban->used && (!ban->until || ban->until > now);
}

static int same_address(const Ban *ban, const uint8_t *address)
{
    return ban->has_address && address && !memcmp(ban->address, address, 4);
}

int ban_find(const BanTable *table, uint32_t now, int32_t account, const uint8_t *address)
{
    int i;
    for (i = 0; i < BAN_MAX; ++i) {
        const Ban *ban = &table->ban[i];
        if (!ban_active(ban, now)) continue;
        if ((account >= 0 && ban->account == account) || same_address(ban, address)) return i;
    }
    return -1;
}

int ban_add(BanTable *table, uint32_t now, const Ban *ban)
{
    int i, slot = -1;
    if (ban->account >= 0)
        for (i = 0; i < BAN_MAX && slot < 0; ++i)
            if (ban_active(&table->ban[i], now) && table->ban[i].account == ban->account) slot = i;
    for (i = 0; i < BAN_MAX && slot < 0; ++i)
        if (!ban_active(&table->ban[i], now)) slot = i;
    if (slot < 0) return -1;
    table->ban[slot] = *ban;
    table->ban[slot].used = 1;
    return slot;
}

int ban_remove(BanTable *table, int slot)
{
    int used;
    if (slot < 0 || slot >= BAN_MAX) return 0;
    used = (int)table->ban[slot].used;
    memset(&table->ban[slot], 0, sizeof table->ban[slot]);
    return used;
}

void ban_left(const Ban *ban, uint32_t now, char *text, unsigned capacity)
{
    uint32_t left;
    if (!ban->until) { snprintf(text, capacity, "perm"); return; }
    left = ban->until > now ? ban->until - now : 0;
    if (left >= 86400) snprintf(text, capacity, "%ud", left / 86400);
    else if (left >= 3600) snprintf(text, capacity, "%uh", left / 3600);
    else snprintf(text, capacity, "%um", (left + 59) / 60);
}
