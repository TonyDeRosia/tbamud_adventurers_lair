#3700
DARKWATER - Keep Hollow Wildlife Below~
2 g 100
~
if !%actor.is_pc% && !%actor.master%
  return 0
end
~
#3701
DARKWATER - Crossroads Arrival~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @cThe noise of Midgaard thins into dripping water and deep stone echoes.@n
%send% %actor% The passage opens into a hollow that feels older than the city above it.
~
#3702
DARKWATER - Deepwell Descent Warning~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @CCold air climbs from below carrying the smell of drowned stone.@n
%send% %actor% The ladder descends into the Lower Underworks, where even Darkwater delvers tread carefully.
~
#3703
DARKWATER - Dead Drop Discovery~
2 g 100
~
if !%actor.is_pc%
  halt
end
%send% %actor% @yFresh black wax marks the boards beside the ironbound cache.@n
%send% %actor% Someone still uses this ledge, and they expect to return.
~
#3704
DARKWATER - Basin Ambience~
2 b 5
~
switch %random.6%
  case 1
    %echo% A slow ripple crosses the blackwater with no wind to drive it.
  break
  case 2
    %echo% Pale fish flash beneath the lantern reflections and vanish.
  break
  case 3
    %echo% Somewhere underwater, stone grinds softly against stone.
  break
  case 4
    %echo% A drop falls from the cavern roof and rings across the basin.
  break
  case 5
    %echo% The current tugs at old chains hidden below the surface.
  break
  case 6
    %echo% A distant oar knocks once against wood, then all is quiet.
  break
done
~
#3705
DARKWATER - Rat Colony Personality~
0 b 5
~
if %self.fighting%
  halt
end
switch %random.5%
  case 1
    emote pauses to gnaw a pale groove into the old mortar.
  break
  case 2
    emote drags a button toward a crack in the foundation.
  break
  case 3
    emote lifts its muzzle and tests the damp air.
  break
  case 4
    emote gives a warning squeak toward an unseen burrow.
  break
  case 5
    emote paws through the silt for something edible.
  break
done
~
#3706
DARKWATER - Duskwing Colony Personality~
0 b 5
~
if %self.fighting%
  halt
end
switch %random.5%
  case 1
    emote shifts its claws along the buried stone above.
  break
  case 2
    emote opens its wings with a dry leathery whisper.
  break
  case 3
    emote answers a distant chirp from deeper in the roost.
  break
  case 4
    emote drops into the air, circles once, and climbs again.
  break
  case 5
    emote turns toward a sound too faint for human ears.
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
emote rises from the heap of stolen tribute, scarred shoulders rolling under dark fur.
%send% %actor% @RGr@ri@Rst@rle@Rfa@rng@n watches you with the cold patience of a king defending his court.
~
#3708
DARKWATER - Keep Roost Wildlife Below~
2 q 100
~
if %direction% == up
  if %actor.vnum% == 3704 || %actor.vnum% == 3705 || %actor.vnum% == 3706 || %actor.vnum% == 3707
    return 0
  end
end
~
#3709
DARKWATER - Keep Surface Wildlife Above~
2 q 100
~
if %direction% == down
  if %actor.vnum% == 3711 || %actor.vnum% == 3712 || %actor.vnum% == 3713
    return 0
  end
end
~
#3710
DARKWATER - Upper Hollow Echoes~
2 b 6
~
switch %random.5%
  case 1
    %echo% Far above, a wagon rumbles across a street you cannot see.
  break
  case 2
    %echo% Water clicks through the old foundations in a dozen tiny streams.
  break
  case 3
    %echo% A rat disappears through a crack with something bright in its teeth.
  break
  case 4
    %echo% Dust sifts from the ceiling as a bell sounds somewhere in Midgaard.
  break
  case 5
    %echo% A shuttered lantern flickers briefly far down another passage.
  break
done
~
#3711
DARKWATER - Gristlefang Warrens~
2 b 6
~
switch %random.4%
  case 1
    %echo% The warren erupts in squeaks, then falls silent all at once.
  break
  case 2
    %echo% Something large pushes through earth behind the wall.
  break
  case 3
    %echo% A stolen spoon clatters across the stone without explanation.
  break
  case 4
    %echo% Warm animal musk rolls through the buried cellar.
  break
done
~
#3712
DARKWATER - Duskwing Gallery Echoes~
2 b 6
~
switch %random.4%
  case 1
    %echo% A wave of wingbeats passes across the ceiling like sudden rain.
  break
  case 2
    %echo% A thin shriek echoes through the buried stone ribs.
  break
  case 3
    %echo% Guano patters softly onto the dark water.
  break
  case 4
    %echo% Something with broad wings brushes the darkness overhead.
  break
done
~
#3713
DARKWATER - Smugglers Westway~
2 b 6
~
switch %random.5%
  case 1
    %echo% A hooded lantern opens once in the distance, then snaps dark.
  break
  case 2
    %echo% Two soft whistles answer one another somewhere along the westway.
  break
  case 3
    %echo% Black sealing wax glistens on a recently moved stone.
  break
  case 4
    %echo% Bootsteps stop around the next bend and do not resume.
  break
  case 5
    %echo% A quiet voice mutters, then the rushing spring swallows the words.
  break
done
~
#3714
DARKWATER - Sunken Commons~
2 b 5
~
switch %random.5%
  case 1
    %echo% Lanternlight reveals the top of a drowned doorway below the water.
  break
  case 2
    %echo% Bubbles rise in a straight line across the sunken paving.
  break
  case 3
    %echo% A pale shell closes somewhere below with a hollow snap.
  break
  case 4
    %echo% The current moves through submerged streets with a low sigh.
  break
  case 5
    %echo% For an instant, the drowned fountain looks almost whole beneath the surface.
  break
done
~
#3715
DARKWATER - Hollowmouth Weather~
2 b 8
~
switch %random.4%
  case 1
    %echo% Wind pushes the smell of Midgaard smoke down the western slope.
  break
  case 2
    %echo% A fox barks somewhere among the old road stones.
  break
  case 3
    %echo% Loose gravel rattles down toward the hidden mouth of the hollow.
  break
  case 4
    %echo% Far below, travelers move along the Western Highway.
  break
done
~
#3716
DARKWATER - Old Midgaard Mason Marks~
2 b 8
~
%echo% A bead of cold water traces one ancient mason mark before vanishing into the silt.
~
#3720
TOMAN VALE - Greeting~
0 h 100
~
if !%actor.is_pc%
  halt
end
say Keep your boots dry and your fingers out of holes. The rats know both are edible.
emote checks the spring on a small iron trap.
~
#3721
TOMAN VALE - Work Personality~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote marks a rat trail with a short line of chalk.
  break
  case 2
    say Gristlefang takes shiny things. That makes him clever enough for me.
  break
  case 3
    emote oils a trap hinge until it moves without a sound.
  break
  case 4
    say If the stone changes from brick to blocks, you are going deeper than the city remembers.
  break
done
~
#3722
DARKWATER - Lantern Scavenger Personality~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote holds a bent nail up to the lantern and pockets it anyway.
  break
  case 2
    emote taps a wall twice and listens for a hollow answer.
  break
  case 3
    say Old stone pays better than new stone if you know who collects it.
  break
  case 4
    emote shuts the lantern and waits until distant footsteps pass.
  break
done
~
#3723
DARKWATER - Smuggler Personality~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote checks the black wax on a buckle before moving on.
  break
  case 2
    emote gives two quiet whistles and listens for an answer.
  break
  case 3
    say The Watch guards gates. We use roads they forgot.
  break
  case 4
    emote lowers the hooded lantern until only one amber slit remains.
  break
done
~
#3724
GUTTERKNIFE - Fight Personality~
0 k 25
~
switch %random.4%
  case 1
    say Should have stayed where the bells can hear you.
  break
  case 2
    emote feints toward the water and cuts back toward dry footing.
  break
  case 3
    say Blacklamp does not own every shadow down here.
  break
  case 4
    emote kicks black silt toward your eyes.
  break
done
~
#3725
MAELA BLACKLAMP - Greeting~
0 h 100
~
if !%actor.is_pc%
  halt
end
emote closes the waterproof ledger without losing her place.
say You can walk the westway if you can keep your head, your light, and your questions to yourself.
~
#3726
MAELA BLACKLAMP - Quartermaster Personality~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote counts black-wax seals against entries in the ledger.
  break
  case 2
    say The old road reaches farther than Midgaard remembers. Not farther than I remember.
  break
  case 3
    emote adjusts the shutter on a lantern until no light escapes sideways.
  break
  case 4
    say Rusk owns the skiff. The basin owns Rusk. Best not argue with either.
  break
done
~
#3727
HOLLOW BRIGAND - Fight Personality~
0 k 25
~
switch %random.3%
  case 1
    say No Watch down here. No witnesses either.
  break
  case 2
    emote braces on the old causeway and drives forward.
  break
  case 3
    emote tries to force you off the dry stones and into the current.
  break
done
~
#3728
BLACKWATER REAVER - Fight Personality~
0 k 25
~
switch %random.3%
  case 1
    emote wades straight through the current without slowing.
  break
  case 2
    say The commons keep what falls in.
  break
  case 3
    emote slams a weapon against drowned stone and sends water spraying.
  break
done
~
#3729
SILTBOUND SENTINEL - Vigil~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote turns its sealed helm toward the deepwell without a sound.
  break
  case 2
    emote sheds a thin sheet of black water from one armored shoulder.
  break
  case 3
    emote drags one ancient boot across stone with a grinding rasp.
  break
done
~
#3730
OLD HOOK RUSK - Greeting~
0 h 100
~
if !%actor.is_pc%
  halt
end
say Darkwater looks still because it likes surprises. Keep a hand on the boat.
emote taps the mooring post twice with his pole.
~
#3731
OLD HOOK RUSK - Ferryman Tales~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    say Found a street sign under the commons once. Same name as a street above, wrong century.
  break
  case 2
    emote spits into the water and watches the current take it east.
  break
  case 3
    say Pearls are pretty. Shellbacks are patient. Remember which one can bite your hand.
  break
  case 4
    emote checks the skiff rope by feel without looking down.
  break
done
~
#3732
SILTBOUND WARDEN - Challenge~
0 h 100
~
if !%actor.is_pc%
  halt
end
wait 1 sec
emote raises a blackwater mace as pearl-white marks flare across ancient armor.
%send% %actor% @CThe current around the Warden runs backward for one heartbeat.@n
~
#3733
SILTBOUND WARDEN - Fight Personality~
0 k 30
~
switch %random.4%
  case 1
    emote drives the mace into stone with a thunderous crack.
  break
  case 2
    %echo% Pearl-white ward marks blaze across the Warden's cuirass.
  break
  case 3
    emote advances through the current like a walking gate.
  break
  case 4
    %echo% Blackwater spirals around the Warden's boots and snaps outward.
  break
done
~
#3734
VASKA BLACKLAMP - Warning~
0 h 100
~
if !%actor.is_pc%
  halt
end
say Wrong ledge. Wrong night. Turn around.
emote lowers the point of a drawn blade toward the narrow footing.
~
#3735
VASKA BLACKLAMP - Fight Personality~
0 k 30
~
switch %random.4%
  case 1
    say Maela keeps books. I settle accounts.
  break
  case 2
    emote uses the narrow ledge to keep you away from the cache.
  break
  case 3
    say You should have taken the warning.
  break
  case 4
    emote snaps a black wax seal from the hilt and lets it fall into the water.
  break
done
~
#3736
OLD-CITY DELVER - Masonry Notes~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote rubs charcoal across an ancient mason mark.
  break
  case 2
    say Midgaard brick on top. Older block beneath. Something older beneath that.
  break
  case 3
    emote measures the width of a drowned paving stone and writes it down.
  break
done
~
#3737
MIRECLOAK SCOUT - Quiet Signals~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote touches two fingers to the wall and listens.
  break
  case 2
    emote opens the lantern shutter just enough to flash a single signal.
  break
  case 3
    emote checks a cord tied low across the passage.
  break
done
~
#3738
HOLLOW HERBALIST - Gathering~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote trims a pale glowcap and lays it carefully in a wicker tray.
  break
  case 2
    say River mint for fever. Glowcap for light. Cavefish oil for everything that squeaks.
  break
  case 3
    emote smells a wet root, frowns, and throws it back into the channel.
  break
done
~
#3739
LANTERN THIEF - Scavenger Personality~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote tries a mismatched key in a rusted lock.
  break
  case 2
    emote pockets a brass screw with an innocent expression.
  break
  case 3
    say If it was abandoned, it was asking to be taken.
  break
done
~
#3740
BLACKWATER APOTHECARY - Basin Lore~
0 b 7
~
if %self.fighting%
  halt
end
switch %random.3%
  case 1
    emote holds a jar of mineral salts against the lanternlight.
  break
  case 2
    say Darkwater grows useful things if you stop calling all of them disgusting.
  break
  case 3
    emote seals a vial of pale oil with black wax.
  break
done
~
$~
