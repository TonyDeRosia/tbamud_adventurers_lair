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
if !%self.fighting%
  switch %random.8%
    case 1
      emote crouches to study a footprint, then scratches a warning mark into the bark.
    break
    case 2
      emote freezes at a distant sound and slowly lowers one hand toward a crude blade.
    break
    case 3
      emote checks a strip of cloth tied around a low branch and quietly changes direction.
    break
    case 4
      say Big tracks again. Too big. Always too big lately.
    break
    case 5
      emote sniffs the wind, grimaces, and spits into the leaves.
    break
    case 6
      say Keep low. Loud feet get eaten first.
    break
    case 7
      emote peers through the brush, then gestures sharply for silence.
    break
    case 8
      say Roads are for folk who want to be seen.
    break
  done
end
~
#3507
Cindervale - Emberfang Reaver Ambient~
0 b 8
~
if !%self.fighting%
  switch %random.8%
    case 1
      emote rolls one broad shoulder and tests the edge of the jagged cleaver with a claw.
    break
    case 2
      emote bares yellow teeth in a humorless grin at something only he can hear.
    break
    case 3
      emote kicks aside a broken branch and deliberately leaves the trail scarred behind him.
    break
    case 4
      say Weak things hide. Strong things take.
    break
    case 5
      emote snorts and pounds one fist against his scarred chest.
    break
    case 6
      say This trail belongs to whoever can keep it.
    break
    case 7
      emote drags the cleaver edge across a stone with a harsh scrape.
    break
    case 8
      say If something wants this ground, let it come argue.
    break
  done
end
~
#3508
Cindervale - Emberfang Hunter Ambient~
0 b 10
~
if !%self.fighting%
  switch %random.9%
    case 1
      emote lowers his muzzle to the earth, following a scent across the trail before looking up sharply.
    break
    case 2
      emote touches two claw marks on a tree and studies how fresh the sap still looks.
    break
    case 3
      emote pauses with one ear turned toward the deeper woods.
    break
    case 4
      emote tests the wind and quietly circles downwind.
    break
    case 5
      say Something wounded passed here.
    break
    case 6
      emote checks a bone hook at his belt and disappears briefly into the brush.
    break
    case 7
      say The forest tells you plenty if you stop making noise.
    break
    case 8
      emote studies a snapped twig as carefully as another hunter might study a map.
    break
    case 9
      say Fresh scent. Not far.
    break
  done
end
~
#3509
Cindervale - Bristlehide Feral Ambient~
0 b 9
~
if !%self.fighting%
  switch %random.8%
    case 1
      emote tears a strip of bark from a fire-scarred trunk and grinds it beneath one heavy hoof.
    break
    case 2
      emote roots angrily through the undergrowth, scattering stones and dead leaves.
    break
    case 3
      emote scrapes one chipped tusk against a tree trunk.
    break
    case 4
      emote snorts hard enough to send dust puffing from the trail.
    break
    case 5
      say Move or be moved.
    break
    case 6
      emote shoulders through a thorn bush without seeming to notice it.
    break
    case 7
      emote stamps once and glares at the nearest movement.
    break
    case 8
      say Too much talking. Not enough moving.
    break
  done
end
~
#3510
Cindervale - Carrionwing Scavenger Ambient~
0 b 12
~
if !%self.fighting%
  switch %random.10%
    case 1
      emote cocks its feathered head and quickly palms some tiny glittering scrap from the ground.
    break
    case 2
      emote turns a bent copper bit over between two clawed fingers.
    break
    case 3
      emote watches the nearest pack with open professional interest.
    break
    case 4
      emote hops onto a stump for a better view of the trail.
    break
    case 5
      say Shiny things belong to whoever sees them first.
    break
    case 6
      emote tucks a colored shard of glass somewhere beneath its ragged mantle.
    break
    case 7
      say Dead folk rarely complain about missing buttons.
    break
    case 8
      emote clicks its beak twice while considering something in the dirt.
    break
    case 9
      say If you dropped it, clearly you did not need it.
    break
    case 10
      emote glances from one traveler to another as if silently estimating resale value.
    break
  done
end
~
#3511
Cindervale - Cinder Wyvern Predator Ambient~
0 b 8
~
if !%self.fighting%
  switch %random.9%
    case 1
      emote spreads its soot-dark wings and lashes its hooked tail, black venom glistening on the spurs.
    break
    case 2
      emote lowers its horned head and tastes the air with a forked tongue.
    break
    case 3
      emote gives a dry rasping hiss and flexes its talons against the earth.
    break
    case 4
      emote snaps its jaws at a drifting leaf and shreds it without effort.
    break
    case 5
      emote suddenly looks skyward, muscles tightening beneath rust-red scales.
    break
    case 6
      emote drags one venom spur across a stone, leaving a dark wet line.
    break
    case 7
      emote circles restlessly, watching every nearby movement like prey.
    break
    case 8
      emote folds its wings tightly and crouches with predatory patience.
    break
    case 9
      emote releases a low clicking growl from deep in its throat.
    break
  done
end
~
$~
