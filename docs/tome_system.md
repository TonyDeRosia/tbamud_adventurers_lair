# Tome system

`ITEM_TOME` is object type 24, appended after FOUNTAIN. Its four ordinary
object values are ability IDs for slots 1 through 4; `0` means empty. A separate
prototype field, persisted as `C` in the object record, stores the builder-set
study cooldown in real seconds. It is deliberately separate from object timer.

In OEDIT choose type TOME, then `N` for Tome Properties. Slots accept an exact
or unambiguous existing ability name, or an ID; `0` clears a slot. The submenu
rejects duplicate/invalid abilities and sets the cooldown. OEDIT saves the
extra cooldown record with the existing object prototype.

Studying a carried Tome validates the entire prototype before changing a player.
All newly unknown abilities are permanently marked and start at 1 percent. The
Tome is consumed once and its own configured cooldown is saved as an absolute
player timestamp. `tome status` and `tome list` show player information.
