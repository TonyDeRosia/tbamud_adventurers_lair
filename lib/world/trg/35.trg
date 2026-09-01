#3500
Dump - 3578~
2 h 100
~
%echo% %object.shortdesc% vanishes in a puff of smoke!
%send% %actor% You are awarded for outstanding performance.
%echoaround% %actor% %actor.name% has been awarded for being a good citizen.
eval value %object.cost% / 10
%purge% %object%
if %value% > 50
  set value 50
elseif %value% < 1
  set value 1
end
if %actor.level% < 3
  nop %actor.exp(%value%)%
else
  nop %actor.gold(%value%)%
end
~
#3501
Cindervale - Tomas Blackbriar Survivor~
0 d 1
riders blackbriar attack ambush father~
if %actor.is_pc%
  emote tightens both hands around the little greenwood charm.
  say Riders. Dark scarves and thorn marks. They came out of the west before we could turn the cart.
  say I heard one of them shout to follow the briar trail. The hoofprints still go that way.
  say Then something screamed farther off in the woods. Even the riders looked afraid for a moment.
end
~
#3502
Cindervale - Harlan Road Rumors~
0 d 1
roads road cindervale goblins goblin gnolls gnoll wyverns wyvern blackbriar riders~
if %actor.is_pc%
  emote rests both scarred hands on the bar and studies %actor.name%.
  say Cindervale has three different troubles, and folk keep making the mistake of calling them one.
  say Greenblood goblins know the old paths, but Emberfang gnolls have been muscling into their warrens beneath Embercrag.
  say The Blackbriar riders are human thieves. Follow the hoof trail south of the inn if you mean to hunt them.
  say And the wyverns? They kill goblins, riders, travelers, anything. Whatever stirred them up, nobody here knows for certain.
end
~
#3503
Cindervale - Corren Dreary Hollow Rumors~
0 d 1
dreary hollow bloodleaf wyvern wyverns dagger carcass caravan~
if %actor.is_pc%
  emote stares into his untouched cup for several long seconds.
  say I saw the clearing after the first caravan vanished. Blades had been used there before the great claw marks appeared.
  say Goblin cuts. Human bootprints. Then punctures deep enough to pass through leather and bone.
  say Someone swore there was silver hidden among the dead, but I never went close enough to prove it.
  say Do not let anyone tell you one creature caused every corpse in Dreary Hollow. The signs disagree.
end
~
#3504
Cindervale - Vargash Intruder Greet~
0 g 100
~
if %actor.is_pc%
  emote lifts his scarred muzzle from the charcoal map and fixes %actor.name% with a yellow-eyed stare.
  say So. Midgaard sends another blade into Embercrag.
  say The goblins hid beneath this mountain for generations. I taught them what happens when stronger jaws find the same den.
  say The vale can burn, the riders can steal, and the winged beasts can feed. Emberfang territory will still grow.
end
~
#3505
Cindervale - Blackbriar Road Ambush~
0 g 55
~
if %actor.is_pc%
  emote shifts a hand toward the hilt of the dark roadblade.
  say Wrong trail, traveler. Drop the pack and you might still walk back to Midgaard.
end
~
#3506
Cindervale - Greenblood Forest Ambient~
0 b 9
~
emote crouches to study a footprint, then scratches a warning mark into the bark.
~
#3507
Cindervale - Emberfang Reaver Ambient~
0 b 8
~
emote rolls one broad shoulder and tests the edge of the jagged cleaver with a claw.
~
#3508
Cindervale - Emberfang Hunter Ambient~
0 b 10
~
emote lowers his muzzle to the earth, following a scent across the trail before looking up sharply.
~
#3509
Cindervale - Bristlehide Feral Ambient~
0 b 9
~
emote tears a strip of bark from a fire-scarred trunk and grinds it beneath one heavy hoof.
~
#3510
Cindervale - Carrionwing Scavenger Ambient~
0 b 12
~
emote cocks its feathered head and quickly palms some tiny glittering scrap from the ground.
~
#3511
Cindervale - Cinder Wyvern Predator Ambient~
0 b 8
~
emote spreads its soot-dark wings and lashes its hooked tail, black venom glistening on the spurs.
~
$~
