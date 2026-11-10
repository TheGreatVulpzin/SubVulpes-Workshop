#include "../bans_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static Ban make(int32_t account, const uint8_t *address, uint32_t until)
{
    Ban ban;
    memset(&ban, 0, sizeof ban);
    ban.account = account;
    if (address) { ban.has_address = 1; memcpy(ban.address, address, 4); }
    ban.until = until;
    return ban;
}

static void durations(void)
{
    uint32_t seconds = 99;
    CHECK(ban_duration("perm", &seconds) && seconds == 0);
    CHECK(ban_duration("forever", &seconds) && seconds == 0);
    CHECK(ban_duration("30m", &seconds) && seconds == 1800);
    CHECK(ban_duration("12h", &seconds) && seconds == 43200);
    CHECK(ban_duration("7d", &seconds) && seconds == 604800);
    CHECK(ban_duration("2w", &seconds) && seconds == 1209600);
    CHECK(ban_duration("3650d", &seconds) && seconds == 3650u * 86400u);
    CHECK(!ban_duration("3651d", &seconds));
    CHECK(!ban_duration("0m", &seconds));
    CHECK(!ban_duration("30", &seconds));
    CHECK(!ban_duration("m", &seconds));
    CHECK(!ban_duration("10x", &seconds));
    CHECK(!ban_duration("10mm", &seconds));
    CHECK(!ban_duration("-5m", &seconds));
    CHECK(!ban_duration("", &seconds));
    CHECK(!ban_duration("99999999999999999999d", &seconds));
    CHECK(!ban_duration(NULL, &seconds));
    CHECK(!ban_duration("5m", NULL));
}

static void finding(void)
{
    static BanTable table;
    const uint8_t home[4] = { 10, 0, 0, 7 }, other[4] = { 10, 0, 0, 8 };
    Ban ban = make(5, home, 0);
    int slot;
    memset(&table, 0, sizeof table);
    CHECK(ban_find(&table, 1000, 5, home) == -1);
    slot = ban_add(&table, 1000, &ban);
    CHECK(slot == 0 && table.ban[0].used);
    CHECK(ban_find(&table, 1000, 5, NULL) == 0);
    CHECK(ban_find(&table, 1000, -1, home) == 0);
    CHECK(ban_find(&table, 1000, 6, other) == -1);
    CHECK(ban_find(&table, 1000, -1, NULL) == -1);
    /* A ban with no address never matches an address. */
    ban = make(6, NULL, 0);
    CHECK(ban_add(&table, 1000, &ban) == 1);
    CHECK(ban_find(&table, 1000, 9, other) == -1);
    CHECK(ban_find(&table, 1000, 6, NULL) == 1);
}

static void ending(void)
{
    static BanTable table;
    Ban ban = make(3, NULL, 2000);
    char text[16];
    memset(&table, 0, sizeof table);
    CHECK(ban_add(&table, 1000, &ban) == 0);
    CHECK(ban_find(&table, 1999, 3, NULL) == 0);
    CHECK(ban_find(&table, 2000, 3, NULL) == -1);
    /* An ended slot is free for a new ban. */
    ban = make(4, NULL, 0);
    CHECK(ban_add(&table, 3000, &ban) == 0);
    ban_left(&table.ban[0], 3000, text, sizeof text);
    CHECK(!strcmp(text, "perm"));
    ban = make(4, NULL, 3000 + 90 * 60);
    ban_left(&ban, 3000, text, sizeof text);
    CHECK(!strcmp(text, "1h"));
    ban.until = 3000 + 2 * 86400 + 5;
    ban_left(&ban, 3000, text, sizeof text);
    CHECK(!strcmp(text, "2d"));
    ban.until = 3000 + 61;
    ban_left(&ban, 3000, text, sizeof text);
    CHECK(!strcmp(text, "2m"));
}

static void replacing_and_limits(void)
{
    static BanTable table;
    Ban ban = make(8, NULL, 5000);
    int i;
    memset(&table, 0, sizeof table);
    CHECK(ban_add(&table, 1000, &ban) == 0);
    /* A new ban for the same account replaces the old one. */
    ban.until = 0;
    CHECK(ban_add(&table, 1000, &ban) == 0);
    CHECK(table.ban[0].until == 0 && !table.ban[1].used);
    for (i = 1; i < BAN_MAX; ++i) {
        ban = make(100 + i, NULL, 0);
        CHECK(ban_add(&table, 1000, &ban) == i);
    }
    ban = make(999, NULL, 0);
    CHECK(ban_add(&table, 1000, &ban) == -1);
    CHECK(ban_remove(&table, 5));
    CHECK(!ban_remove(&table, 5));
    CHECK(!ban_remove(&table, -1));
    CHECK(!ban_remove(&table, BAN_MAX));
    CHECK(ban_add(&table, 1000, &ban) == 5);
}

int main(void)
{
    CHECK(sizeof(Ban) == 120);
    durations();
    finding();
    ending();
    replacing_and_limits();
    puts("bans table: all checks passed");
    return 0;
}
