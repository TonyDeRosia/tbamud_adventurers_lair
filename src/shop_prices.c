#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "shop_prices.h"

float shop_charisma_discount(const struct char_data *ch)
{
  if (!ch || IS_NPC(ch) || GET_LEVEL(ch) >= LVL_IMMORT)
    return 1.0f;

  int cha = GET_CHA(ch);

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
  float price = base_cost * buyprofit;
  long final_price = (long)(price * shop_charisma_discount(buyer) + 0.5f);

  (void)keeper_cha;

#ifdef SHOP_DISCOUNT_DEBUG
  if (buyer && !IS_NPC(buyer)) {
    char buf[MAX_INPUT_LENGTH];
    float discount = shop_charisma_discount(buyer);
    long profit_price = (long)(price + 0.5f);

    snprintf(buf, sizeof(buf),
             "Base cost: %ld | Profit price: %ld | CHA: %d | Discount: %.2f | Final: %ld\r\n",
             base_cost, profit_price, GET_CHA(buyer), discount, final_price);
    send_to_char(buyer, "%s", buf);
  }
#endif

  if (final_price < 1)
    final_price = 1;

  return final_price;
}
