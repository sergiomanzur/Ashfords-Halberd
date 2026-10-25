/* Exercise the game's real search/combine/sort routines and retail item tables.
   Regression: ammo must be found in every inventory position, independently of
   the unrelated status flag which happens to share the expansion bit number. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/sub1.h>
#include <ps2/veronica/prog/item.h>
#include <stdio.h>
#include <string.h>

static SYS_WORK state;
SYS_WORK* sys = &state;
static unsigned int inventory[12];
static int failures, cases;
BH_PWORK ply;
BH_PWORK* plp = &ply;
static int button;
int pad_reload(void) { return button; }
int pc_inf_ammo(void) { return 0; }
void reload_player(void);

static void check(int ok, const char* what, int weapon, int slot, int expanded)
{
    if (!ok) {
        if (failures < 30) fprintf(stderr, "FAIL %s weapon=%d ammo-slot=%d expanded=%d\n", what, weapon, slot, expanded);
        failures++;
    }
}

static void reset(int expanded, int status_expanded)
{
    memset(&state, 0, sizeof(state));
    memset(&swork, 0, sizeof(swork));
    memset(inventory, 0, sizeof(inventory));
    sys->gm_flg = expanded ? 0x8000000 : 0;
    sys->st_flg = status_expanded ? 0x8000000 : 0;
    swork.pip = inventory;
    memset(&ply, 0, sizeof(ply));
    button = 0;
}

static void position_case(int weapon, int ammo, int capacity, int expanded,
                          int status_expanded, int weapon_slot, int ammo_slot,
                          int rounds, int loaded, int use_button, int mode)
{
    int slots = expanded ? 10 : 8;
    unsigned int want = loaded + rounds < capacity ? loaded + rounds : capacity;
    unsigned int remaining = rounds - (want - loaded), found = 0;
    reset(expanded, status_expanded);
    sys->gm_mode = mode;
    /* Real key items fill the gaps: sorting/consumption must preserve them. */
    for (int i = 0; i < slots; ++i) inventory[i + 2] = (43u << 16) | 1;
    if (itemdata[weapon].type & 0x100) inventory[slots + 1] = 0;
    inventory[0] = weapon_slot + 2;
    inventory[weapon_slot + 2] = ((unsigned int)weapon << 16) | loaded;
    inventory[ammo_slot + 2] = ((unsigned int)ammo << 16) | rounds;
    if (use_button) {
        /* A normal raised, ready weapon. Motion pointers are only checked for
           presence by the glue; the animation controller is outside this test. */
        sys->sp_flg = 0x40;
        ply.mnwP = (void*)&state;
        ply.mlwP = (void*)&state;
        ply.mode0 = 1; ply.mode1 = 1; ply.mode2 = 65; ply.mode3 = 1;
        ply.at_flg = 0x10;
        static int extension;
        if (use_button == 2) {
            ply.mode1 = 0; ply.mode2 = 0; ply.mode3 = 0;
            ply.flg = 0x20000; ply.exp1 = (unsigned char*)&extension; extension = 0;
        }
        /* Retail weapon motion number for the corresponding inventory item. */
        ply.wpnr_no = weapon == 32 ? 14 : weapon;
        reload_player(); /* release previous press */
        button = 1;
        reload_player();
        check(ply.mode2 == 70 && ply.mode3 == 0, "button enters reload", weapon, ammo_slot, expanded);
    } else {
        check(bhSearchBullet() == 1, "search", weapon, ammo_slot, expanded);
    }
    check((inventory[inventory[0]] >> 16) == (unsigned int)weapon,
          "equipped weapon preserved", weapon, ammo_slot, expanded);
    check((inventory[inventory[0]] & 0xffff) == want,
          "magazine", weapon, ammo_slot, expanded);
    for (int i = 2; i < 12; ++i)
        if ((inventory[i] >> 16) == (unsigned int)ammo) found += inventory[i] & 0xffff;
    check(found == remaining, "reserve conservation", weapon, ammo_slot, expanded);
    cases++;
}

int main(void)
{
    /* Keep production reload diagnostics out of an exhaustive matrix's output. */
#ifdef _WIN32
    freopen("NUL", "w", stdout);
#else
    freopen("/dev/null", "w", stdout);
#endif
    /* Hand-checked retail IDs and magazine sizes (item.c), rather than fixture
       tables which could conceal an incorrect weapon/ammo compatibility. */
    static const int pairs[][3] = {
        {5, 12, 18}, {9, 12, 15}, {10, 12, 20}, /* handguns */
        {4, 14, 7}, {3, 40, 7},                /* bow gun, shotgun */
        {32, 13, 6}, {7, 18, 30}               /* magnum, assault rifle */
    };
    for (unsigned p = 0; p < sizeof(pairs) / sizeof(pairs[0]); ++p)
        for (int expanded = 0; expanded <= 1; ++expanded)
            for (int status = 0; status <= 1; ++status)
                for (int weapon = 0; weapon < (expanded ? 10 : 8); ++weapon)
                    for (int ammo = 0; ammo < (expanded ? 10 : 8); ++ammo)
                        if (weapon != ammo &&
                            (!(itemdata[pairs[p][0]].type & 0x100) ||
                             (weapon == 0 && ammo < (expanded ? 9 : 7)))) {
                            for (int mode = 0; mode < 4; ++mode)
                                for (int button_path = 0; button_path < 3; ++button_path) {
                                    position_case(pairs[p][0], pairs[p][1], pairs[p][2], expanded, status, weapon, ammo, 50, 2, button_path, mode);
                                    position_case(pairs[p][0], pairs[p][1], pairs[p][2], expanded, status, weapon, ammo, 3, 0, button_path, mode);
                                }
                        }
    /* An unexpanded inventory must not inspect words belonging to the two
       locked slots, even when the unrelated status bit is set. */
    reset(0, 1);
    inventory[0] = 2;
    inventory[2] = 5u << 16;
    inventory[10] = (12u << 16) | 20;
    check(bhSearchBullet() == 0 && inventory[2] == (5u << 16),
          "locked slot ignored", 5, 8, 0);
    reset(0, 0);
    inventory[0] = 2;
    inventory[2] = 5u << 16;
    inventory[3] = (40u << 16) | 20;
    check(bhSearchBullet() == 0 && inventory[3] == ((40u << 16) | 20),
          "incompatible ammo unchanged", 5, 1, 0);
    /* Infinite reserve remains an infinite reserve after filling the gun. */
    reset(0, 0);
    inventory[0] = 2;
    inventory[2] = (5u << 16) | 2;
    inventory[7] = (0x80cu << 16) | 15;
    check(bhSearchBullet() == 1 && inventory[2] == ((5u << 16) | 18) &&
          inventory[3] == ((0x80cu << 16) | 15), "infinite reserve", 5, 5, 0);
    fprintf(stderr, "reload: %d cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
