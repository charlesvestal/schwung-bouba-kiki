#ifndef BK_ENVELOPE_H
#define BK_ENVELOPE_H
#include <math.h>
typedef struct bk_adsr {
  float attack, decay, sustain, release;
} bk_adsr_t;
enum {
  BK_ENV_IDLE,
  BK_ENV_ATTACK,
  BK_ENV_DECAY,
  BK_ENV_SUSTAIN,
  BK_ENV_RELEASE
};
typedef struct bk_envelope {
  float level, release_start;
  int gate, stage;
} bk_envelope_t;
static inline void bk_envelope_gate(bk_envelope_t *e, int gate) {
  if (gate && !e->gate)
    e->stage = BK_ENV_ATTACK;
  else if (!gate && e->gate) {
    e->release_start = e->level;
    e->stage = BK_ENV_RELEASE;
  }
  e->gate = gate != 0;
}
static inline float bk_envelope_tick(bk_envelope_t *e, const bk_adsr_t *p,
                                     float rate) {
  const float sustain = fmaxf(0, fminf(1, p->sustain));
  switch (e->stage) {
  case BK_ENV_ATTACK:
    e->level = fminf(1, e->level + 1 / fmaxf(1, rate * p->attack));
    if (e->level >= 1)
      e->stage = BK_ENV_DECAY;
    break;
  case BK_ENV_DECAY:
    e->level =
        fmaxf(sustain, e->level - (1 - sustain) / fmaxf(1, rate * p->decay));
    if (e->level <= sustain)
      e->stage = BK_ENV_SUSTAIN;
    break;
  case BK_ENV_SUSTAIN:
    e->level = sustain;
    break;
  case BK_ENV_RELEASE:
    e->level =
        fmaxf(0, e->level - e->release_start / fmaxf(1, rate * p->release));
    if (e->level <= 0) {
      e->level = 0;
      e->stage = BK_ENV_IDLE;
    }
    break;
  default:
    e->level = 0;
    break;
  }
  return e->level;
}
#endif
