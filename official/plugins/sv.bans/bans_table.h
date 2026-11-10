#ifndef SV_BANS_TABLE_H
#define SV_BANS_TABLE_H

#include <stdint.h>

/* The ban table. It holds no pointer and calls no host function, so a native test can run it alone. */
#define BAN_MAX 128
#define BAN_NAME 24
#define BAN_REASON 48
#define BAN_DAYS_MAX 3650

/* One ban. An account ban has an account number. An address ban has the address. Both can be set. until is a time in
 * seconds since 1970, or 0 for a ban with no end. */
typedef struct {
    uint32_t used;
    int32_t account;
    uint32_t has_address;
    uint8_t address[4];
    uint32_t created, until;
    char by[BAN_NAME], name[BAN_NAME], reason[BAN_REASON];
} Ban;

typedef struct {
    Ban ban[BAN_MAX];
} BanTable;

/* Reads "perm" or a number with a unit: 30m, 12h, 7d or 2w. Gives the length in seconds, or 0 for perm. 1 when the text is
 * a duration and 0 when it is not. A length over BAN_DAYS_MAX days is not a duration. */
int ban_duration(const char *text, uint32_t *seconds);

/* 1 when the ban is in force at this time. */
int ban_active(const Ban *ban, uint32_t now);

/* The slot of the first active ban for this account or for this address, or -1. The account is -1 for none. The address
 * may be NULL. */
int ban_find(const BanTable *table, uint32_t now, int32_t account, const uint8_t *address);

/* Stores the ban in the slot of an active ban for the same account, or in a free or ended slot. Gives the slot, or -1 when
 * the table is full. */
int ban_add(BanTable *table, uint32_t now, const Ban *ban);

/* Clears one slot. 1 when the slot held a ban. */
int ban_remove(BanTable *table, int slot);

/* Writes the time that is left: "perm", "3d", "5h" or "12m". */
void ban_left(const Ban *ban, uint32_t now, char *text, unsigned capacity);

#endif
