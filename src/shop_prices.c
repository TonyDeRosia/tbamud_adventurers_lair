#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "shop_prices.h"

float shop_charisma_discount(const struct char_data *buyer, int keeper_cha)
{
  if (!buyer || IS_NPC(buyer) || GET_LEVEL(buyer) >= LVL_IMMORT)
    return 1.0f;

  int cha_diff = keeper_cha - GET_CHA(buyer);

  if (cha_diff > 15)
    cha_diff = 15;
  else if (cha_diff < -15)
    cha_diff = -15;

  return 1.0f + (cha_diff / 70.0f);
}

long shop_calculate_buy_price(long base_cost, float buyprofit, int keeper_cha, const struct char_data *buyer)
{
  float price = base_cost * buyprofit;
  long final_price = (long)(price * shop_charisma_discount(buyer, keeper_cha) + 0.5f);

  (void)keeper_cha;

#ifdef SHOP_DISCOUNT_DEBUG
  if (buyer && !IS_NPC(buyer)) {
    char buf[MAX_INPUT_LENGTH];
    float discount = shop_charisma_discount(buyer, keeper_cha);
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
