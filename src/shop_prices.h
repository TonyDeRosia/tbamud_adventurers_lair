#ifndef SHOP_PRICES_H
#define SHOP_PRICES_H

#include "structs.h"

float shop_charisma_discount(const struct char_data *ch);
long shop_calculate_buy_price(long base_cost, float buyprofit, int keeper_cha, const struct char_data *buyer);

#endif /* SHOP_PRICES_H */
