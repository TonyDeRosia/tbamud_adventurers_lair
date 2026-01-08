#ifndef SHOP_PRICES_H
#define SHOP_PRICES_H

#include "structs.h"


#define SHOP_PRICE_SCALE_DIV 1000
#define SHOP_PRICE_SCALE_MIN 1000

long shop_scale_base_cost(long base_cost);
float shop_charisma_discount(const struct char_data *buyer, int keeper_cha);
long shop_calculate_buy_price(long base_cost, float buyprofit, int keeper_cha, const struct char_data *buyer);

#endif /* SHOP_PRICES_H */
