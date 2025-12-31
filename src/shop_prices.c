#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "shop_prices.h"

float shop_charisma_discount(const struct char_data *ch)
{
  int cha = GET_CHA(ch);

  if (!ch || IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return 1.0f;

  if (cha <= 13)
    return 1.0f;

  if (cha > 25)
    cha = 25;

  /* CHA 13 -> 1.00, CHA 25 -> 0.85 (max 15% discount) */
  {
    float t = (float)(cha - 13) / (float)(25 - 13);
    return 1.0f - (0.15f * t);
  }
}

long shop_calculate_buy_price(long base_cost, float buyprofit, int keeper_cha, const struct char_data *buyer)
{
  float price = base_cost * buyprofit * (1 + (keeper_cha - GET_CHA(buyer)) / 70.0f);
  long final_price = (long)(price * shop_charisma_discount(buyer) + 0.5f);

  if (final_price < 1)
    final_price = 1;

  return final_price;
}
