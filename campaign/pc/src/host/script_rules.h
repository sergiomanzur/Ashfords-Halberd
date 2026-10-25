#ifndef ASHFORD_SCRIPT_RULES_H
#define ASHFORD_SCRIPT_RULES_H
#include <stdint.h>
enum { ATR_WAL, ATR_ETC, ATR_FLR, ATR_OBJ };
typedef struct AtrSet { uint8_t kind, ix, type, prm[4]; uint32_t attr; } AtrSet;
int pc_insn_len_raw(const uint8_t* p);
int pc_op_cond(int op);
int pc_atr_set(const uint8_t* p, AtrSet* out);
int pc_room_key(void);
#endif
