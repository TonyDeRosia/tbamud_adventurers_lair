#3800
LOWERWORKS - Sluice-Maw Gnawing~
0 b 3
~
if %self.fighting%
halt
end
switch %random.4%
case 1
emote braces its forepaws against the grate and worries at the iron with worn teeth.
break
case 2
emote jerks its head up and gives a low territorial hiss.
break
case 3
emote paws through a nest of rusted tools and old rope.
break
case 4
emote tests the corroded key hanging at its neck with one dirty claw.
break
done
~
#3801
SLAIVE - Concealed Safe Settling~
1 b 3
~
switch %random.3%
case 1
%echo% The concealed iron safe gives a faint metallic tick as old metal settles.
break
case 2
%echo% Dust slips from the false masonry around the hidden safe.
break
case 3
%echo% Somewhere inside the old safe, a loose coin shifts with a tiny scrape.
break
done
~
#3802
NIGHTNEEDLE - Cold Grip~
1 j 100
~
%send% %actor% @DThe narrow grip of Nightneedle feels unnaturally cold in your hand.@n
%echoaround% %actor% @DNightneedle seems to drink a little of the light as %actor.name% takes it in hand.@n
~
#3803
LOWERWORKS - Ossuary Spillway Ambience~
2 b 3
~
switch %random.5%
case 1
%echo% A fragment of bone turns slowly in the dark central current.
break
case 2
%echo% Water whispers through a cracked burial niche and disappears beneath the floor.
break
case 3
%echo% Something wet scrapes briefly against stone farther down the spillway.
break
case 4
%echo% A breath of cold air moves through the old funerary arches.
break
case 5
%echo% Mineral water drips from the vault and strikes the channel with a hollow note.
break
done
~
#3804
SLAIVE - Restless Awakening~
0 h 100
~
if !%actor.is_pc%
halt
end
wait 1 sec
emote lies perfectly still for one heartbeat too long.
wait 1 sec
emote closes one dry hand around Nightneedle and rises with the practiced balance of a living duelist.
%send% %actor% @RThe dead knifemaster fixes you with his ruined gaze.@n
wait 1 sec
mkill %actor%
~
#3805
LOWERWORKS - Sealed Threshold Warning~
2 g 100
~
if !%actor.is_pc%
halt
end
%send% %actor% @DThe old burial script around the sealed doorway is worn, but the newer warning marks are unmistakable.@n
%send% %actor% Someone deliberately sealed the catacombs beyond this chamber and ordered later workers not to reopen them.
~
#3806
SLAIVE - Refuge Discovery~
2 g 100
~
if !%actor.is_pc%
halt
end
%send% %actor% @YThe hidden room is no animal burrow. Maps, coded tallies, and a concealed safe mark this as a thief's long-used refuge.@n
~
#3807
LOWERWORKS - Pale Undertaker Presence~
0 h 100
~
if !%actor.is_pc%
halt
end
wait 1 sec
emote turns its smooth funerary mask toward %actor.name% without making a sound.
%send% %actor% @DThe Pale Undertaker moves with the deliberate certainty of a guardian that has waited centuries for this moment.@n
~
$~
