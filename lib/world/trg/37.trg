#3700
block_mobs_not_following~
2 g 100
~
if !%actor.is_pc% && !%actor.master%
  return 0
end
~
#3701
UNDERWORKS - Main Sluice Arrival~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @cThe noise of Midgaard fades behind layers of brick and running water.@n
%send% %actor% @DThe upper underworks smell of wet stone, old iron, and everything the city would rather forget.@n
~
#3702
UNDERWORKS - Deepworks Descent Warning~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @CCold air rises from the shaft below.@n
%send% %actor% Old maintenance marks warn that the lower works are older, less stable, and no longer part of the city's maintained sewer.
~
#3703
UNDERWORKS - Hidden Cache Discovery~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @YThe ledge is too deliberate to be natural. Somebody used this violent little chamber as a hiding place.@n
%send% %actor% Scrape marks around the boards and ironbound cache suggest the smugglers expected to return.
~
#3704
UNDERWORKS - Reservoir Ambience~
2 b 3
~
switch %random.5%
  case 1
    %echo% Ripples pass across the dark reservoir without revealing what disturbed them.
  break
  case 2
    %echo% A drop falls from the cavern roof and rings across the water like a tiny bell.
  break
  case 3
    %echo% Pale fish flash beneath the surface and vanish into deeper water.
  break
  case 4
    %echo% The slow underground current brushes cold against the stone.
  break
  case 5
    %echo% Somewhere below, a heavy shell snaps shut with a hollow clack.
  break
done
~
#3705
UNDERWORKS - Rat Colony Ambience~
0 b 3
~
if %self.fighting%
  halt
end
switch %random.5%
  case 1
    emote pauses to gnaw at something too small to identify.
  break
  case 2
    emote lifts its muzzle and tests the air for food.
  break
  case 3
    emote scratches rapidly at the mortar between two bricks.
  break
  case 4
    emote gives a sharp warning squeak toward an unseen tunnel.
  break
  case 5
    emote drags a scrap of cloth toward the nearest crack in the masonry.
  break
done
~
#3706
UNDERWORKS - Bat Colony Ambience~
0 b 3
~
if %self.fighting%
  halt
end
switch %random.5%
  case 1
    emote shifts its grip on the damp ceiling.
  break
  case 2
    emote opens its wings and gives a dry leathery flutter.
  break
  case 3
    emote emits a thin chirp that is answered from deeper in the roost.
  break
  case 4
    emote drops from the ceiling, circles once, and climbs back into the shadows.
  break
  case 5
    emote turns its head toward a sound too faint for human ears.
  break
done
~
#3707
GRISTLEFANG - King Below Presence~
0 h 100
~
if !%actor.is_pc%
  halt
end
wait 1 sec
emote rises from the heap of stolen tribute, immense shoulders rolling beneath scarred fur.
%send% %actor% @RFor one uncomfortable moment, Gristlefang studies you with the stillness of something that has survived many challengers.@n
~
#3708
UNDERWORKS - Keep Roost Wildlife Below~
2 q 100
~
* Preserve the habitat boundary without making bats SENTINEL.
* Bats may wander naturally through the sewer, but not out through the vent.
if %direction% == up
  if %actor.vnum% == 3704 || %actor.vnum% == 3705 || %actor.vnum% == 3706 || %actor.vnum% == 3707
    return 0
  end
end
~
#3709
UNDERWORKS - Keep Surface Wildlife Above~
2 q 100
~
* Preserve the habitat boundary without making surface wildlife SENTINEL.
* Hares, mousers, and foxes may roam outside, but not descend into the sewer.
if %direction% == down
  if %actor.vnum% == 3711 || %actor.vnum% == 3712 || %actor.vnum% == 3713
    return 0
  end
end
~
$~
