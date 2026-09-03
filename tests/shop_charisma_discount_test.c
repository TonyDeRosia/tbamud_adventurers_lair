#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "shop_prices.h"
#include <math.h>
#include "utils.h"

#ifdef log
#undef log
#endif

static int expect_long_eq(const char *label, long expected, long actual)
{
  if (expected != actual) {
    fprintf(stderr, "%s: expected %ld but got %ld\n", label, expected, actual);
    return 1;
  }
  return 0;
}

static int expect_float_close(const char *label, float expected, float actual, float tolerance)
{
  float delta = fabsf(expected - actual);
  if (delta > tolerance) {
    fprintf(stderr, "%s: expected %.4f but got %.4f (delta %.4f)\n", label, expected, actual, delta);
    return 1;
  }
  return 0;
}

static void init_character(struct char_data *ch, int cha, int level, bool is_npc)
{
  memset(ch, 0, sizeof(*ch));
  ch->aff_abils.cha = cha;
  ch->player.level = level;
  if (is_npc)
    SET_BIT_AR(MOB_FLAGS(ch), MOB_ISNPC);
}

int main(void)
{
  struct char_data buyer, keeper;
  int failures = 0;

  init_character(&buyer, 13, 1, FALSE);
  init_character(&keeper, 11, 1, TRUE);

  init_character(&keeper, 13, 1, TRUE);

  failures += expect_float_close("baseline discount", 1.0f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 14, 1, FALSE);
  init_character(&keeper, 11, 1, TRUE);
  failures += expect_float_close("CHA 14 discount", 0.9833333f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 18, 1, FALSE);
  failures += expect_float_close("CHA 18 discount", 0.9166667f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 20, 1, FALSE);
  failures += expect_float_close("CHA 20 discount", 0.8833333f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 25, 1, FALSE);
  failures += expect_float_close("CHA 25 discount cap", 0.8000f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 30, LVL_IMMORT, FALSE);
  failures += expect_float_close("immortal has no discount", 1.0f, shop_charisma_discount(&buyer, GET_CHA(&keeper)), 0.0001f);

  init_character(&buyer, 18, 1, FALSE);
  long price = shop_calculate_buy_price(SHOP_PRICE_SCALE_DIV * 1000L, 1.0f, GET_CHA(&keeper), &buyer);
  failures += expect_long_eq("charisma-adjusted price", 917, price);

  long undiscounted = shop_calculate_buy_price(SHOP_PRICE_SCALE_DIV * 1000L, 1.0f, GET_CHA(&keeper), NULL);
  failures += expect_long_eq("no buyer leaves price unchanged", 1000, undiscounted);

  init_character(&buyer, 25, 1, FALSE);
  long high_cha_price = shop_calculate_buy_price(SHOP_PRICE_SCALE_DIV * 1000L, 1.0f, GET_CHA(&keeper), &buyer);
  failures += expect_long_eq("higher charisma decreases price", 800, high_cha_price);

  return failures;
}
