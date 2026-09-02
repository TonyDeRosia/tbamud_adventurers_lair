#3000
RETIRED Former Mage Guildguard Compatibility~
0 q 100
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3001
RETIRED Former Cleric Guildguard Compatibility~
0 q 100
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3002
RETIRED Former Thief Guildguard Compatibility~
0 q 100
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3003
RETIRED Former Warrior Guildguard Compatibility~
0 q 100
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3004
Dump - 3030~
2 h 100
~
%send% %actor% You are awarded for outstanding performance.
%echoaround% %actor% %actor.name% has been awarded for being a good citizen.
eval value %object.cost% / 10
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
%purge% %object%
~
#3005
RETIRED Stock Thief~
0 b 10
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3006
RETIRED Stock Snake~
0 k 10
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3007
RETIRED Stock Magic User~
0 k 10
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
#3008
Near Death Trap~
2 g 100
~
* By Rumble of The Builder Academy    tbamud.com 9091
* Near Death Trap stuns actor
set stunned %actor.hitp%
%damage% %actor% %stunned%
%send% %actor% You are on the brink of life and death.
%send% %actor% The Gods must favor you this day.
~
#3009
Midgaard City Watch~
0 b 50
~
* Northern Midgaard civic watch response.
* Scan everyone present instead of choosing a random character.
if %self.fighting%
  halt
end
set here %self.room.people%
while %here%
  set next %here.next_in_room%
  if %here.is_pc%
    if %here.is_killer%
      switch %random.14%
        case 1
          say Murderer! Stop where you are!
        break
        case 2
          say City watch! Drop the weapon!
        break
        case 3
          say You have spilled enough blood. Stand down!
        break
        case 4
          say Halt! You are wanted for murder!
        break
        case 5
          say Not another step! Surrender!
        break
        case 6
          say Watch! Murderer in the street!
        break
        case 7
          say Throw down your weapon and submit to the law!
        break
        case 8
          say You will answer for those deaths!
        break
        case 9
          say Stop running and face the law!
        break
        case 10
          say Stand down before anyone else gets hurt!
        break
        case 11
          say Murderer! The city has had enough of you!
        break
        case 12
          say Surrender now or be taken by force!
        break
        case 13
          say Hold there! You are under arrest!
        break
        case 14
          say The watch sees you. Stand down!
        break
      done
      kill %here.name%
      halt
    elseif %here.is_thief%
      switch %random.16%
        case 1
          say STOP! THIEF!
        break
        case 2
          say Hold it right there!
        break
        case 3
          say Hands where I can see them!
        break
        case 4
          say Thief! Do not make this a chase!
        break
        case 5
          say Drop what you took and surrender!
        break
        case 6
          say Watch! Thief in the market!
        break
        case 7
          say You picked a poor place to steal!
        break
        case 8
          say Stop now and this stays simple!
        break
        case 9
          say The city has rules about other people's property!
        break
        case 10
          say Put it back and stand down!
        break
        case 11
          say Thief! Stop before you make this worse!
        break
        case 12
          say Running will only tire both of us!
        break
        case 13
          say You are caught. Surrender!
        break
        case 14
          say Halt! Theft earns a hearing and possibly a cell!
        break
        case 15
          say Keep your hands visible and your feet still!
        break
        case 16
          say Stop! The watch wants a word with you!
        break
      done
      kill %here.name%
      halt
    end
  end
  if (%here.vnum% == 3059 || %here.vnum% == 3060 || %here.vnum% == 3067) && %here.fighting%
    switch %random.8%
      case 1
        say Watch, lend a hand!
      break
      case 2
        say I have you!
      break
      case 3
        say Keep them contained!
      break
      case 4
        say Coming through!
      break
      case 5
        say Hold your ground!
      break
      case 6
        say Watch, together!
      break
      case 7
        say Do not let them bolt!
      break
      case 8
        say I am with you!
      break
    done
    assist %here.name%
    halt
  end
  set here %next%
done
~
#3010
Stray Hound Scavenging~
0 b 100
~
* Stray hounds clean up corpses left in the streets.
set inroom %self.room%
set item %inroom.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum(65535)%
    emote tears hungrily at a discarded corpse.
    %purge% %item%
    halt
  end
  set item %next_item%
done
~
#3011
Street Scavenger Cleanup~
0 b 100
~
* Street scavengers clean up cheap debris left around Midgaard.
eval inroom %self.room%
eval item %inroom.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.type% != FOUNTAIN && %item.cost% <= 15
    take %item.name%
  end
  set item %next_item%
done
~
#3012
Newcomer's City Guide~
0 e 0
has entered the game.~
* Newcomer guidance for redesigned Northern Midgaard.
if %actor.is_pc%
  say Welcome to Midgaard, %actor.name%. For training, practice, contracts, and adventuring work, visit the Adventurer's Guild on Guild Way east of the market.
end
~
#3013
Newcomer's Guide Welcome~
0 e 0
has entered the game.~
* By Rumble of The Builder Academy    tbamud.com 9091
* Num Arg 0 means the argument has to match exactly. So trig will only fire off:
* "has entered game." and not "has" or "entered" etc. (that would be num arg 1).
* Figure out what vnum the mob is in so we can use zoneecho.
eval inroom %self.room%
%zoneecho% %inroom.vnum% %self.name% shouts, 'Welcome, %actor.name%!'
~
#3014
Wayfarer's Rune Passage - Sealed~
1 c 3
teleport~
* Adventurer's Lair: the stock world-directory teleport network is retired.
* The rune-stone now keeps only its homeward recall/return attunement on T3015.
%send% %actor% The rune-stone warms briefly, but its distant passage sigils remain dark.
%send% %actor% Only the homeward and returning marks are still attuned.
%echoaround% %actor% Pale runes stir across the stone, then settle without opening a passage.
halt
~
#3015
Wayfarer's Rune Recall and Return~
1 c 7
re~
* By Rumble of The Builder Academy    tbamud.com 9091
if %cmd% == recall
  eval waystone_return_room %actor.room.vnum%
  remote waystone_return_room %actor.id%
  %send% %actor% You invoke the rune-stone's homeward sigil.
  %echoaround% %actor% A ring of pale runes rises around %actor.name%.
  %teleport% %actor% 3001
  %force% %actor% look
  %echoaround% %actor% A circle of silver runes fades as %actor.name% steps into view.
elseif %cmd% == return
  %send% %actor% You invoke the rune-stone's returning sigil.
  %echoaround% %actor% Silver runes rise around %actor.name% and fold inward.
  %teleport% %actor% %actor.waystone_return_room%
  %force% %actor% look
  %echoaround% %actor% A circle of silver runes fades as %actor.name% steps into view.
else
  return 0
end
~
#3016
Kind Soul Gives Newcomer Equipment~
0 g 100
~
* By Rumble of The Builder Academy    tbamud.com 9091
* If a player is < level 5 and naked it fully equips them. If < 5 and missing
* some equipment it will equip one spot.
if %actor.is_pc% && %actor.level% < 5
  wait 2 sec
  if !%actor.eq(*)%
    say get some clothes on! Here, I will help.
    %load% obj 3037 %actor% light
    %load% obj 3083 %actor% rfinger
    %load% obj 3083 %actor% lfinger
    %load% obj 3082 %actor% neck1
    %load% obj 3082 %actor% neck2
    %load% obj 3040 %actor% body
    %load% obj 3076 %actor% head
    %load% obj 3080 %actor% legs
    %load% obj 3084 %actor% feet
    %load% obj 3071 %actor% hands
    %load% obj 3086 %actor% arms
    %load% obj 3042 %actor% shield
    %load% obj 3087 %actor% about
    %load% obj 3088 %actor% waist
    %load% obj 3089 %actor% rwrist
    %load% obj 3089 %actor% lwrist
    %load% obj 3021 %actor% wield
    %load% obj 3055 %actor% hold
    halt
  end
  if !%actor.eq(light)%
    say you really shouldn't be wandering these parts without a light source %actor.name%.
    shake
    %load% obj 3037
    give candle %actor.name%
    halt
  end
  if !%actor.eq(rfinger)% || !%actor.eq(lfinger)%
    say did you lose one of your rings?
    sigh
    %load% obj 3083
    give ring %actor.name%
    halt
  end
  if !%actor.eq(neck1)% || !%actor.eq(neck2)%
    say you lose everything don't you?
    roll
    %load% obj 3082
    give neck %actor.name%
    halt
  end
  if !%actor.eq(body)%
    say you won't get far without some body armor %actor.name%.
    %load% obj 3040
    give plate %actor.name%
    halt
  end
  if !%actor.eq(head)%
    say protect that noggin of yours, %actor.name%.
    %load% obj 3076
    give cap %actor.name%
    halt
  end
  if !%actor.eq(legs)%
    say why do you always lose your pants %actor.name%?
    %load% obj 3080
    give leggings %actor.name%
    halt
  end
  if !%actor.eq(feet)%
    say you can't go around barefoot %actor.name%.
    %load% obj 3084
    give boots %actor.name%
    halt
  end
  if !%actor.eq(hands)%
    say need some gloves %actor.name%?
    %load% obj 3071
    give gloves %actor.name%
    halt
  end
  if !%actor.eq(arms)%
    say you must be freezing %actor.name%.
    %load% obj 3086
    give sleeve %actor.name%
    halt
  end
  if !%actor.eq(shield)%
    say you need one of these to protect yourself %actor.name%.
    %load% obj 3042
    give shield %actor.name%
    halt
  end
  if !%actor.eq(about)%
    say you are going to catch a cold %actor.name%.
    %load% obj 3087
    give cape %actor.name%
    halt
  end
  if !%actor.eq(waist)%
    say better use this to hold your pants up %actor.name%.
    %load% obj 3088
    give belt %actor.name%
    halt
  end
  if !%actor.eq(rwrist)% || !%actor.eq(lwrist)%
    say misplace something?
    smile
    %load% obj 3089
    give wristguard %actor.name%
    halt
  end
  if !%actor.eq(wield)%
    say without a weapon you will be Fido food %actor.name%.
    %load% obj 3021
    give sword %actor.name%
    halt
  end
end
~
#3017
Mortal Greet~
2 s 100
~
* By Rumble of The Builder Academy    tbamud.com 9091
* TBA mortal greet and equip. New players start at level 0.
wait 1 sec
if %actor.level% == 0
  if !%actor.eq(*)%
    %load% obj 3037 %actor% light
    %load% obj 3083 %actor% rfinger
    %load% obj 3083 %actor% lfinger
    %load% obj 3082 %actor% neck1
    %load% obj 3082 %actor% neck2
    %load% obj 3040 %actor% body
    %load% obj 3076 %actor% head
    %load% obj 3080 %actor% legs
    %load% obj 3084 %actor% feet
    %load% obj 3071 %actor% hands
    %load% obj 3086 %actor% arms
    %load% obj 3042 %actor% shield
    %load% obj 3087 %actor% about
    %load% obj 3088 %actor% waist
    %load% obj 3089 %actor% rwrist
    %load% obj 3089 %actor% lwrist
    %load% obj 3021 %actor% wield
    %load% obj 3055 %actor% hold
  end
  if !%actor.has_item(3006)%
    %load% obj 3006 %actor% inv
  end
end
wait 3 sec
%zoneecho% 3001 A booming voice announces, 'Welcome %actor.name% to the realm!'
~
#3020
Zone30 Peacekeeper Civic Ambient~
0 b 4
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote watches the crowd rather than any one person.
    break
    case 2
      emote shifts aside to let a hurried messenger pass.
    break
    case 3
      emote checks a shop doorway, then continues scanning the street.
    break
    case 4
      say Keep the lanes open. Carts and tempers both need room.
    break
    case 5
      say If you have business, conduct it. If you have trouble, take it elsewhere.
    break
    case 6
      emote exchanges a brief nod with a passing worker.
    break
    case 7
      say Markets are crowded enough without people making problems.
    break
    case 8
      emote studies the flow of pedestrians through the square.
    break
    case 9
      say Keep your coin close and your hands to yourself.
    break
    case 10
      say A quiet watch is a good watch.
    break
    case 11
      emote flexes one hand and settles back into an easy stance.
    break
    case 12
      say Need directions? Ask before you wander into somewhere private.
    break
    case 13
      emote glances toward the nearest road junction and counts passing carts.
    break
    case 14
      say Behave and we will both have an uneventful day.
    break
  done
end
~
#3021
Zone30 Cityguard Street Ambient~
0 b 4
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote checks the street ahead and then the windows above it.
    break
    case 2
      emote moves a loose crate farther from the roadway with one boot.
    break
    case 3
      say Keep moving if you have somewhere to be.
    break
    case 4
      emote adjusts the sword belt at his hip.
    break
    case 5
      say Mind the carts. Drivers assume everyone else can move faster than they can.
    break
    case 6
      emote pauses to listen to an argument somewhere down the street.
    break
    case 7
      say Keep the peace and the watch will keep out of your business.
    break
    case 8
      emote looks over a nearby doorway as if checking it against memory.
    break
    case 9
      say No fighting in the street. Find a training yard or leave the walls.
    break
    case 10
      emote scans the crowd for a few moments before relaxing slightly.
    break
    case 11
      say Watch your purse in a crowd.
    break
    case 12
      emote steps around a puddle with the resignation of long practice.
    break
    case 13
      say Busy streets work better when everyone remembers they are not the only person on them.
    break
    case 14
      emote checks the nearest alley before returning to the main road.
    break
  done
end
~
#3022
Zone30 Gatewatch Ambient~
0 b 4
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote studies the road beyond the gate for approaching traffic.
    break
    case 2
      emote checks the gate hinges and gives one iron strap a testing shove.
    break
    case 3
      say Keep the entrance clear. Wagons need the whole arch.
    break
    case 4
      emote watches a departing traveler until they are well beyond the walls.
    break
    case 5
      say If you are heading out, carry water and know where the road goes.
    break
    case 6
      emote glances up toward the tower walk.
    break
    case 7
      say Declare dangerous animals before you bring them through the gate.
    break
    case 8
      emote checks the road for mud, hoof prints, and wagon ruts.
    break
    case 9
      say Do not stop beneath the gate unless you enjoy being shouted at by teamsters.
    break
    case 10
      emote shades his eyes and studies something far down the road.
    break
    case 11
      say City is open. Trouble is not.
    break
    case 12
      emote exchanges a hand signal with someone on the wall above.
    break
    case 13
      say Coming in? Keep right and keep moving.
    break
    case 14
      emote gives the gate mechanism a quick visual inspection.
    break
  done
end
~
#3023
Zone30 Temple Warden Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote lowers his voice as footsteps pass through the cloister.
    break
    case 2
      emote checks that the passage remains clear for petitioners and pilgrims.
    break
    case 3
      say Sanctuary is not an excuse for disorder.
    break
    case 4
      emote straightens a small lamp near the wall.
    break
    case 5
      say Speak softly here. Some people came seeking peace.
    break
    case 6
      emote watches the temple entrance with patient attention.
    break
    case 7
      say Leave weapons sheathed unless there is genuine danger.
    break
    case 8
      emote steps aside for an elderly visitor without taking his eyes off the room.
    break
    case 9
      say Aid is given here. Do not make the work harder.
    break
    case 10
      emote checks a doorway and returns to his post.
    break
    case 11
      say There is room for questions. There is less room for shouting.
    break
    case 12
      emote folds his hands behind his back and resumes his quiet watch.
    break
  done
end
~
#3024
Zone30 Arcane Warden Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks a rune-marked boundary around a demonstration circle.
    break
    case 2
      emote studies a faint shimmer in the air and makes a small notation.
    break
    case 3
      say If the ward changes color, stop touching whatever you are touching.
    break
    case 4
      emote taps a cabinet seal with the end of a wand.
    break
    case 5
      say Controlled experimentation begins with the word controlled.
    break
    case 6
      emote watches a drifting spark until it fades.
    break
    case 7
      say Ask before opening anything marked with three warning sigils.
    break
    case 8
      emote checks the floor for chalk marks that have been scuffed out of place.
    break
    case 9
      say Curiosity is useful. So are eyebrows. Try to keep both.
    break
    case 10
      emote adjusts one warding token by less than a finger's width.
    break
    case 11
      say If it starts whispering your name, set it down.
    break
    case 12
      emote quietly surveys the hall for unattended magical objects.
    break
  done
end
~
#3025
Zone30 Guild Bounty Hunter Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote studies a sealed contract without breaking the wax.
    break
    case 2
      emote checks the edge on a well-kept blade with one thumb.
    break
    case 3
      say A good contract tells you what the client forgot to mention.
    break
    case 4
      emote glances toward the alley and then back to the postings.
    break
    case 5
      say Bounties are easy. Finding the right person is the work.
    break
    case 6
      emote compares two descriptions and quietly shakes his head.
    break
    case 7
      say Never trust a sketch made by someone who was running at the time.
    break
    case 8
      emote checks a knot on one of the sealed bounty tokens at his belt.
    break
    case 9
      say If the reward sounds generous, read the danger clause twice.
    break
    case 10
      emote watches everyone entering the annex with professional suspicion.
    break
    case 11
      say Some people hide. Some people just choose roads nobody else wants to walk.
    break
    case 12
      emote folds a notice and tucks it into his coat.
    break
  done
end
~
#3026
Zone30 Guild Warden Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks the guild registry and returns it to the desk.
    break
    case 2
      say Contracts inside. Arguments outside.
    break
    case 3
      emote watches a group of adventurers pass with the expression of someone counting weapons.
    break
    case 4
      say If you need training, use the yard. If you need work, read the board.
    break
    case 5
      emote straightens the crossed-road badge at his shoulder.
    break
    case 6
      say The guild welcomes every path. It does not welcome every behavior.
    break
    case 7
      emote checks the entrance and then the corridor beyond it.
    break
    case 8
      say Sign the registry before claiming guild privileges.
    break
    case 9
      emote moves an abandoned pack away from the doorway.
    break
    case 10
      say Keep the entrance clear. Returning parties tend to carry more than they left with.
    break
    case 11
      emote gives a battered shield an approving glance as its owner passes.
    break
    case 12
      say A guild hall is not a tavern. Usually.
    break
  done
end
~
#3027
Zone30 Master Courier Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote sorts three sealed letters into separate pigeonholes without hesitation.
    break
    case 2
      emote checks a wax seal against a mark in the dispatch ledger.
    break
    case 3
      say Names clearly written save everyone trouble.
    break
    case 4
      emote ties a small packet with fresh cord and adds a route tag.
    break
    case 5
      say Urgent does not mean unreadable.
    break
    case 6
      emote taps a finger down a list of outgoing routes.
    break
    case 7
      say Bad weather delays horses. Bad addresses delay everyone.
    break
    case 8
      emote weighs a parcel in both hands before marking the ledger.
    break
    case 9
      say Sealed letters stay sealed. That rule is not negotiable.
    break
    case 10
      emote stacks dispatches in the order their riders are expected to leave.
    break
    case 11
      say If the recipient moved, tell us before the courier learns it the hard way.
    break
    case 12
      emote sands a fresh line of ink and closes the ledger.
    break
  done
end
~
#3028
Zone30 City Guide Ambient~
0 b 6
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote straightens a hand-drawn map and smooths one stubborn corner.
    break
    case 2
      say Market in the center, guild to the east, temple north. Start with those and you will manage.
    break
    case 3
      emote points out a route on the map to an imaginary traveler, rehearsing it under her breath.
    break
    case 4
      say If you get lost, find the market. Half the city eventually passes through it.
    break
    case 5
      emote checks that several spare maps are still tucked into her folio.
    break
    case 6
      say The courier hall is beside the inn entrance. People miss it constantly.
    break
    case 7
      emote adds a tiny note beside an alley on one of her maps.
    break
    case 8
      say The gates are simple. West goes west, east goes east, and somehow people still ask.
    break
    case 9
      emote smiles at a confused passerby before they even ask a question.
    break
    case 10
      say Need work? The Adventurer's Guild keeps notices posted.
    break
    case 11
      emote flips through a folio filled with route notes and local landmarks.
    break
    case 12
      say The quietest shortcut is not always the safest shortcut.
    break
    case 13
      emote traces a route from the temple to the guild with one finger.
    break
    case 14
      say Ask directions before pride carries you three streets the wrong way.
    break
  done
end
~
#3029
Zone30 Guild Steward Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote wipes down a table already cleaner than most tavern tables ever become.
    break
    case 2
      say Eat before the contract. Heroics are harder on an empty stomach.
    break
    case 3
      emote sets aside a chipped mug and reaches for another.
    break
    case 4
      say If you just came back from the Delve, boots stay near the mat.
    break
    case 5
      emote listens to a returning party's story while pretending not to.
    break
    case 6
      say The notice board has work. I have stew. Do not confuse our responsibilities.
    break
    case 7
      emote counts empty seats before preparing another tray.
    break
    case 8
      say Tell the story after you wash the blood off.
    break
    case 9
      emote refills a water pitcher and sets it within easy reach.
    break
    case 10
      say Congratulations on surviving. Your table still needs clearing.
    break
    case 11
      emote glances toward the training yard at the sound of a heavy impact.
    break
    case 12
      say Contracts come and go. Hungry adventurers are forever.
    break
  done
end
~
#3030
Zone30 Grunting Boar Bartender Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote wipes a mug with a cloth that has seen better evenings.
    break
    case 2
      say If you are starting a story with no offense, buy the drink first.
    break
    case 3
      emote nudges a fresh log deeper into the hearth with one boot.
    break
    case 4
      say I serve ale, food, and very limited patience.
    break
    case 5
      emote catches a sliding mug before it reaches the edge of the counter.
    break
    case 6
      say Break a chair and you bought a chair.
    break
    case 7
      emote listens to a traveler's boast and raises one skeptical eyebrow.
    break
    case 8
      say Everyone in here has killed a dragon after the third drink.
    break
    case 9
      emote stacks clean cups beneath the counter.
    break
    case 10
      say Keep your coin dry and your arguments short.
    break
    case 11
      emote checks the room with the practiced eye of someone who knows exactly when a tavern is about to get loud.
    break
    case 12
      say The hearth is free. The drinks are not.
    break
    case 13
      emote slides a bowl away from a patron who has clearly fallen asleep.
    break
    case 14
      say If you cannot remember your room, reception is upstairs.
    break
  done
end
~
#3031
Zone30 Hearthstone Baker Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote turns a cooling loaf and checks the crust with one knuckle.
    break
    case 2
      say Fresh bread first. Sweet things after you pretend you came for bread.
    break
    case 3
      emote dusts flour from the counter with a quick sweep of one hand.
    break
    case 4
      say Travel bread lasts longer if you do not eat it before leaving the gate.
    break
    case 5
      emote checks the oven through a narrow iron hatch.
    break
    case 6
      say Honey glaze is not a meal. I know this because I have tried.
    break
    case 7
      emote moves a pastry tray farther from the counter's edge.
    break
    case 8
      say If you smell burning, tell me. If you smell bread, that is intentional.
    break
    case 9
      emote scores the top of a fresh loaf with a small knife.
    break
    case 10
      say Morning batch is nearly gone. That is usually a good sign.
    break
    case 11
      emote rearranges several loaves by size with unnecessary precision.
    break
    case 12
      say Crumbs are free. Everything larger costs coin.
    break
  done
end
~
#3032
Zone30 General Store Clerk Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks a lantern shutter and returns it to the shelf.
    break
    case 2
      say Rope, light, bags, boxes. Buy the boring things before you need them.
    break
    case 3
      emote restacks a row of travel supplies by size.
    break
    case 4
      say Every adventurer remembers a sword. Half of them forget a light.
    break
    case 5
      emote tightens the stopper on a small container.
    break
    case 6
      say If you are asking whether you need another bag, the answer is probably yes.
    break
    case 7
      emote checks the counter for misplaced merchandise.
    break
    case 8
      say Preparation is cheaper than improvisation.
    break
    case 9
      emote holds a lantern up to the light and inspects the glass.
    break
    case 10
      say Boxes are less exciting than treasure until you have nowhere to put the treasure.
    break
    case 11
      emote makes a small mark in a stock ledger.
    break
    case 12
      say If you break it before buying it, congratulations, you bought it.
    break
  done
end
~
#3033
Zone30 Ironmark Weaponsmith Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote runs a whetstone along a blade and listens to the edge.
    break
    case 2
      say A sharp blade is useful. Knowing when not to draw it is better.
    break
    case 3
      emote checks the balance of a hammer with two short practice motions.
    break
    case 4
      say Weight matters more than shine once a fight starts.
    break
    case 5
      emote wipes a trace of oil from a finished weapon.
    break
    case 6
      say Do not swing that indoors unless you plan to buy the wall too.
    break
    case 7
      emote sights down the length of a blade for any hint of a bend.
    break
    case 8
      say A weapon you can control beats one you can barely lift.
    break
    case 9
      emote adjusts a weapon rack so every hilt sits at the same angle.
    break
    case 10
      say Nicks tell stories. Deep cracks tell you to buy a new weapon.
    break
    case 11
      emote tests a grip wrap and reties it tighter.
    break
    case 12
      say Buy for your hand, not your ego.
    break
  done
end
~
#3034
Zone30 Ironward Armorer Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks the rivets on a mail shirt one row at a time.
    break
    case 2
      say Armor should move with you. If it fights you, it is fitted wrong.
    break
    case 3
      emote tests a leather strap and replaces it without comment.
    break
    case 4
      say Dents are cheaper to fix before they become holes.
    break
    case 5
      emote polishes a helmet just enough to reveal a shallow scratch.
    break
    case 6
      say Try the shoulders again. You were standing differently the first time.
    break
    case 7
      emote compares two gauntlets and swaps their position on the rack.
    break
    case 8
      say Plate is impressive until you cannot climb a stair in it.
    break
    case 9
      emote bends a leather joint repeatedly to work stiffness from it.
    break
    case 10
      say Good armor is supposed to come home scratched.
    break
    case 11
      emote measures a strap against a marked length on the counter.
    break
    case 12
      say Fit first. Decoration later.
    break
  done
end
~
#3035
Zone30 Springwater Merchant Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote holds a bottle up to the light and examines the clarity with satisfaction.
    break
    case 2
      say Clean water is never impressive until you need it.
    break
    case 3
      emote checks the seal on a travel canteen.
    break
    case 4
      say Fill up before the road. Thirst charges worse prices than I do.
    break
    case 5
      emote wipes condensation from a cool clay jug.
    break
    case 6
      say Spring water. No mystery herbs, no glowing sediment, just water.
    break
    case 7
      emote taps a barrel and listens to how full it sounds.
    break
    case 8
      say Travelers remember boots and blades. Smart travelers remember water.
    break
    case 9
      emote rearranges stoppered bottles in a neat row.
    break
    case 10
      say If it smells strange, do not drink it. Mine does not.
    break
    case 11
      emote checks a leather canteen for leaks.
    break
    case 12
      say The road east gets dry faster than people expect.
    break
  done
end
~
#3036
Zone30 Inn Steward Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks a room key against an entry in the ledger.
    break
    case 2
      say Rooms upstairs, tavern below. Try not to confuse the two after midnight.
    break
    case 3
      emote rings the small hand bell once and immediately decides that was unnecessary.
    break
    case 4
      say If you lose a key, tell me before someone else finds it.
    break
    case 5
      emote straightens a row of brass keys behind the counter.
    break
    case 6
      say Mud on the stairs becomes mud in the rooms. Use the mat.
    break
    case 7
      emote checks the stairway and makes a note in the ledger.
    break
    case 8
      say Quiet dreams are easier when the hallway stays quiet too.
    break
    case 9
      emote closes one ledger and opens another without looking down.
    break
    case 10
      say The bell is for service, not entertainment.
    break
    case 11
      emote counts the remaining keys twice.
    break
    case 12
      say If you need directions, ask now. I would rather explain than retrieve you later.
    break
  done
end
~
#3037
Zone30 Travelling Peddler Ambient~
0 b 6
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote reties a bundle hanging from one overloaded pack.
    break
    case 2
      say Useful? Maybe. Interesting? Absolutely.
    break
    case 3
      emote produces a tiny carved charm, considers it, and puts it away again.
    break
    case 4
      say I have crossed three roads to sell things nobody knew they needed.
    break
    case 5
      emote counts a handful of mismatched coins and pockets them.
    break
    case 6
      say Never insult a trinket until you know what somebody else will pay for it.
    break
    case 7
      emote shifts one pack and somehow reveals two more dangling parcels.
    break
    case 8
      say Rare is a flexible word. Let us call this uncommon.
    break
    case 9
      emote checks a waxed wrapping for rain damage.
    break
    case 10
      say Roads teach you what sells. Rain teaches you how to wrap it.
    break
    case 11
      emote smiles brightly at the nearest potential customer.
    break
    case 12
      say Browse freely. Touch carefully.
    break
    case 13
      emote pulls a colored ribbon from a pack, frowns, and pushes it back inside.
    break
    case 14
      say I trade in necessities, curiosities, and things that become necessities after purchase.
    break
  done
end
~
#3038
Zone30 Gloam Alley Mercenary Ambient~
0 b 6
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote leans against the wall while watching everyone who enters the alley.
    break
    case 2
      say Coin first. Details second. Heroics cost extra.
    break
    case 3
      emote checks the point of a small sword and returns it to the sheath.
    break
    case 4
      say Escort work pays better when the client admits what is chasing them.
    break
    case 5
      emote rolls one shoulder against the stone wall.
    break
    case 6
      say I do not ask why. I do ask how dangerous.
    break
    case 7
      emote watches the contract-annex door with calculated patience.
    break
    case 8
      say Cheap work becomes expensive work very quickly.
    break
    case 9
      emote checks a worn buckle on the sword belt.
    break
    case 10
      say If the job is safe, you probably do not need me.
    break
    case 11
      emote glances toward Commons Square and then back into the alley.
    break
    case 12
      say No credit. No promises. No haunting me if it goes badly.
    break
  done
end
~
#3039
Zone30 Grunting Boar Drunk Ambient~
0 b 7
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote squints into an empty cup as if betrayed by it.
    break
    case 2
      say I was absolutely winning that argument. I just forgot what it was about.
    break
    case 3
      emote attempts to sit straighter and nearly succeeds.
    break
    case 4
      say Roads are easier when they stop moving.
    break
    case 5
      emote raises a cup toward nobody in particular.
    break
    case 6
      say To good friends, bad plans, and forgetting which was which.
    break
    case 7
      emote pats one pocket, then another, then looks deeply concerned.
    break
    case 8
      say I had money when I came in. Probably.
    break
    case 9
      emote studies the floor as though it has personally offended him.
    break
    case 10
      say That table moved. I saw it.
    break
    case 11
      emote laughs quietly at a joke nobody else heard.
    break
    case 12
      say I know exactly where I am. Tavern.
    break
    case 13
      emote points at the hearth, misses slightly, and nods anyway.
    break
    case 14
      say One more and I become wise. That is how it works.
    break
  done
end
~
#3040
Zone30 Street Beggar Ambient~
0 b 7
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote cups both hands around a few worn coins and counts them carefully.
    break
    case 2
      say Spare a coin? I promise to spend it on something the temple would approve of.
    break
    case 3
      emote shifts to a drier patch of stone.
    break
    case 4
      say Travelers always look richer before they check their purses.
    break
    case 5
      emote watches the market crowd with sharp, tired eyes.
    break
    case 6
      say A little kindness weighs less than a sword.
    break
    case 7
      emote tucks a frayed blanket more tightly around one shoulder.
    break
    case 8
      say No coin? A bit of bread does not argue.
    break
    case 9
      emote gives a passing guard a respectful amount of space.
    break
    case 10
      say City is generous when the weather is warm.
    break
    case 11
      emote smooths a battered cup against one knee.
    break
    case 12
      say Keep your boots dry if you can. Wet stone is a miserable bed.
    break
  done
end
~
#3041
Zone30 Guild Instructor Ambient~
0 b 5
~
if !%self.fighting%
  switch %random.14%
    case 1
      emote watches a practice swing and makes a small correcting gesture.
    break
    case 2
      say Speed hides mistakes until the mistake matters. Slow it down.
    break
    case 3
      emote nudges a training marker back into place with one boot.
    break
    case 4
      say Practice what fails, not what already makes you look impressive.
    break
    case 5
      emote studies the casting circles and then the weapon lanes.
    break
    case 6
      say Every path has fundamentals. Pride is not one of them.
    break
    case 7
      emote checks the padding on a practice post.
    break
    case 8
      say Control first. Power second.
    break
    case 9
      emote folds his arms and watches an imaginary drill through to its conclusion.
    break
    case 10
      say If you cannot repeat it tired, you do not own the skill yet.
    break
    case 11
      emote gestures toward the climbing frames and marked lanes.
    break
    case 12
      say Train the weakness before an enemy discovers it for you.
    break
    case 13
      emote resets a practice target and steps back.
    break
    case 14
      say Good training is boring right up until it saves your life.
    break
  done
end
~
#3042
Zone30 Kennelmaster Ambient~
0 b 6
~
if !%self.fighting%
  switch %random.12%
    case 1
      emote checks a latch and gives it a second tug for certainty.
    break
    case 2
      say Let them smell your hand. Do not shove it into their face.
    break
    case 3
      emote refills a water bowl and wipes one muddy paw print from the rim.
    break
    case 4
      say A trained animal still has teeth. Respect solves most problems.
    break
    case 5
      emote whistles a short two-note signal toward the holding pens.
    break
    case 6
      say Pick the animal that fits your life, not the one that looks impressive.
    break
    case 7
      emote checks a worn leather lead for cracks.
    break
    case 8
      say Puppies chew. Wolves also chew. The consequences differ.
    break
    case 9
      emote listens to the animals for a moment and seems satisfied.
    break
    case 10
      say Feed them, train them, and do not blame them for what you failed to teach.
    break
    case 11
      emote scratches one notation onto a small kennel slate.
    break
    case 12
      say Calm hands make calmer animals.
    break
  done
end
~
#3043
Zone30 Watch Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %actor.is_killer% || %actor.is_thief%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say This is not the time for conversation. Stand down and answer to the law.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(greetings)% || %speech.contains(morning)% || %speech.contains(evening)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.4%
    case 1
      say Good day. Keep your business peaceful.
    break
    case 2
      emote gives %actor.name% a brief professional nod.
    break
    case 3
      say Welcome to Midgaard. Mind the crowds and you will do fine.
    break
    case 4
      say Evening. Need directions, or just passing through?
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(help)% || %speech.contains(lost)% || %speech.contains(where)% || %speech.contains(direction)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.4%
    case 1
      say Temple Square is north of the market. The Adventurer's Guild lies east along Guild Way.
    break
    case 2
      say For rooms and food, ask at one of the inns. For training and contracts, head for the Adventurer's Guild.
    break
    case 3
      say If you are lost, find the Grand Market first. Most major streets connect back to it.
    break
    case 4
      say Tell me what landmark you are looking for and I may be able to point you the right way.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(temple)% || %speech.contains(heal)% || %speech.contains(healer)% || %speech.contains(priest)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The High Temple is north of Temple Square. If you are hurt, that is where I would go.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(guild)% || %speech.contains(train)% || %speech.contains(practice)% || %speech.contains(contract)% || %speech.contains(quest)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The Adventurer's Guild handles training, practice, and contracts. Follow Guild Way east from the market.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(inn)% || %speech.contains(tavern)% || %speech.contains(sleep)% || %speech.contains(room)% || %speech.contains(rest)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Midgaard has several places to sleep and eat. Ask the city guide if you want the closest one.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(shop)% || %speech.contains(weapon)% || %speech.contains(armor)% || %speech.contains(food)% || %speech.contains(water)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The market district has most essentials. Weapons, armor, provisions, and water are all sold nearby.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(law)% || %speech.contains(rule)% || %speech.contains(crime)% || %speech.contains(thief)% || %speech.contains(murder)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Keep your hands off what is not yours, keep your weapon sheathed unless you need it, and do not start fights in the streets.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(cindervale)% || %speech.contains(south gate)% || %speech.contains(road)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Travelers heading beyond the southern districts should carry water and pay attention to the road. Cindervale is not city ground.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say You are welcome.
    break
    case 2
      emote nods once to %actor.name%.
    break
    case 3
      say Safe travels.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3044
Zone30 Watch Courtesy Act~
0 e 1
waves bows nods salutes smiles~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
set z30_social_lock 1
remote z30_social_lock %actor.id%
switch %random.4%
  case 1
    emote returns %actor.name%'s gesture with a restrained nod.
  break
  case 2
    say Good day.
  break
  case 3
    emote acknowledges %actor.name% without taking attention off the street.
  break
  case 4
    emote offers %actor.name% a brief, professional salute.
  break
done
wait 3 sec
rdelete z30_social_lock %actor.id%
~
#3045
Zone30 Watch Hostility Act~
0 e 1
glares spits kicks punches slaps threatens~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
set z30_social_lock 1
remote z30_social_lock %actor.id%
switch %random.5%
  case 1
    say Easy. Keep this peaceful.
  break
  case 2
    emote squares up slightly and fixes %actor.name% with a warning look.
  break
  case 3
    say Whatever point you are making, make it without starting a fight.
  break
  case 4
    say That is your warning. Settle down.
  break
  case 5
    emote rests one hand near the baton at the belt.
  break
done
wait 3 sec
rdelete z30_social_lock %actor.id%
~
#3046
Zone30 Watch Distress Act~
0 e 1
cries sobs collapses faints groans staggers~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
set z30_social_lock 1
remote z30_social_lock %actor.id%
switch %random.4%
  case 1
    say Are you hurt? The temple is north if you need a healer.
  break
  case 2
    emote studies %actor.name% more closely, checking for obvious injury.
  break
  case 3
    say If you need help, say what happened.
  break
  case 4
    say Sit down before you fall down. Then tell me what you need.
  break
done
wait 3 sec
rdelete z30_social_lock %actor.id%
~
#3047
Zone30 City Guide Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(help)% || %speech.contains(lost)% || %speech.contains(new)% || %speech.contains(start)% || %speech.contains(what do i do)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Start with three things: learn your skills, get basic supplies, and ask the Adventurer's Guild about work.
  say If you tell me what you need, I can narrow that down.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(guild)% || %speech.contains(train)% || %speech.contains(practice)% || %speech.contains(skill)% || %speech.contains(class)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The Adventurer's Guild is the best first stop. Its instructor handles both training and practice for adventurers of every path.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(quest)% || %speech.contains(contract)% || %speech.contains(work)% || %speech.contains(job)% || %speech.contains(money)% || %speech.contains(gold)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Check the Adventurer's Guild for contracts. Merchants and civic workers may also know of smaller opportunities.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(temple)% || %speech.contains(heal)% || %speech.contains(hurt)% || %speech.contains(wounded)% || %speech.contains(pray)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The High Temple stands north of Temple Square. If you are badly hurt, do not wander around looking for a cheaper answer.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(inn)% || %speech.contains(tavern)% || %speech.contains(room)% || %speech.contains(sleep)% || %speech.contains(rest)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say There are inns around the central districts. The Grunting Boar is easy to find from Temple Square, while cheaper rooms can be found farther into the poorer streets.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(weapon)% || %speech.contains(armor)% || %speech.contains(equipment)% || %speech.contains(gear)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The market has dedicated weapons and armor shops. Buy what you can actually use before spending coin on something impressive.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(food)% || %speech.contains(bread)% || %speech.contains(water)% || %speech.contains(drink)% || %speech.contains(supply)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Food, water, and travel supplies are sold around the market. Wally handles water, and the baker is difficult to miss once you smell the ovens.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(cindervale)% || %speech.contains(south)% || %speech.contains(outside)% || %speech.contains(wilderness)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Cindervale lies beyond the southern districts. Do not treat the road outside the walls like another city street.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(greetings)% || %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Glad to help. Midgaard makes more sense once you know its main roads.
    break
    case 2
      emote smiles and adjusts the folio of hand-drawn maps under one arm.
    break
    case 3
      say Ask as many questions as you need. Better that than getting lost outside the walls.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3048
Zone30 Merchant Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(greetings)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Welcome. Have a look around.
    break
    case 2
      emote gives %actor.name% the practiced nod of a merchant sizing up a customer.
    break
    case 3
      say If you need something specific, ask.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(buy)% || %speech.contains(sell)% || %speech.contains(price)% || %speech.contains(cost)% || %speech.contains(shop)% || %speech.contains(wares)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Ask about what I actually stock and I will deal with you plainly. For something outside my trade, another market stall is likely a better choice.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(cheap)% || %speech.contains(expensive)% || %speech.contains(bargain)% || %speech.contains(discount)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Good goods cost coin. Bad goods cost more once they fail.
    break
    case 2
      say You can ask for a bargain. That does not mean you will get one.
    break
    case 3
      emote folds both arms and looks thoroughly unconvinced by the attempted bargaining.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(guild)% || %speech.contains(train)% || %speech.contains(contract)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Adventuring business belongs at the guild. Shopping belongs here. Best not to confuse the two.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(temple)% || %speech.contains(heal)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Temple Square is north of the market. If you are bleeding, go there before worrying about shopping.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Fair travels, and mind your coin purse in a crowd.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3049
Zone30 Hospitality Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(evening)% || %speech.contains(morning)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Welcome. Find yourself a place and settle in.
    break
    case 2
      emote acknowledges %actor.name% with a brief nod while continuing to work.
    break
    case 3
      say Come in. Roads are hard enough without standing in doorways.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(room)% || %speech.contains(sleep)% || %speech.contains(bed)% || %speech.contains(rest)% || %speech.contains(inn)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say If you need a proper room, ask the inn steward. If you only need to sit, eat, or get out of the street, you are already in the right sort of place.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(food)% || %speech.contains(hungry)% || %speech.contains(eat)% || %speech.contains(meal)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Sit down and ask for something sensible. Nobody thinks clearly on an empty stomach.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(drink)% || %speech.contains(thirst)% || %speech.contains(ale)% || %speech.contains(water)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Drinks are easy. Trouble after too many of them is the expensive part.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(rumor)% || %speech.contains(news)% || %speech.contains(heard)% || %speech.contains(trouble)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Travelers talk. Most of it grows in the telling.
    break
    case 2
      say Ask about a road or district and I may have heard something useful.
    break
    case 3
      say The watch hears official trouble. Inns hear everything else.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(fight)% || %speech.contains(brawl)% || %speech.contains(kill)% || %speech.contains(threat)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Take that sort of talk outside, and preferably beyond my door entirely.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say You are welcome. Leave the place no worse than you found it.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3050
Zone30 Guild Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(train)% || %speech.contains(training)% || %speech.contains(practice)% || %speech.contains(skill)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The guild instructor handles training and practice. Learn what your path actually needs before spending every point you have.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(contract)% || %speech.contains(quest)% || %speech.contains(job)% || %speech.contains(work)% || %speech.contains(bounty)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Read contracts carefully. A short description on a board can hide a very long walk and a very dangerous problem.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(new)% || %speech.contains(beginner)% || %speech.contains(start)% || %speech.contains(help)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say New adventurers should train, carry water, keep a light source, and learn the city before chasing distant contracts.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(group)% || %speech.contains(party)% || %speech.contains(team)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say A reliable companion is worth more than another piece of shiny equipment. Know who can hold a line, heal, scout, or get you out alive.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(cindervale)% || %speech.contains(wilderness)% || %speech.contains(road)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Cindervale rewards preparation and punishes assumptions. Track what is around you, and do not chase enemies into ground you have not studied.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Welcome to the guild.
    break
    case 2
      emote gives %actor.name% an appraising nod.
    break
    case 3
      say Keep learning. Experience is only useful if you survive long enough to use it.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3051
Zone30 Temple Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(heal)% || %speech.contains(hurt)% || %speech.contains(wound)% || %speech.contains(dying)% || %speech.contains(injured)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say If you are injured, seek the temple's healing services first. Pride has killed more travelers than monsters ever needed to.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(pray)% || %speech.contains(prayer)% || %speech.contains(god)% || %speech.contains(gods)% || %speech.contains(bless)% || %speech.contains(faith)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Prayer is welcome here. So are questions, provided they are asked with a little patience.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(death)% || %speech.contains(dead)% || %speech.contains(die)% || %speech.contains(grave)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Every life ends. The useful question is what you do before that hour arrives.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(help)% || %speech.contains(lost)% || %speech.contains(temple)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say You are already near the city's spiritual center. For adventuring work, seek the guild. For ordinary supplies, return toward the market.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say Peace to you.
    break
    case 2
      emote inclines the head respectfully toward %actor.name%.
    break
    case 3
      say May your road be longer than your list of regrets.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3052
Zone30 Courier River Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(mail)% || %speech.contains(letter)% || %speech.contains(message)% || %speech.contains(courier)% || %speech.contains(post)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Letters and dispatches move through the courier service. Give a clear destination and do not seal nonsense in an official packet.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(river)% || %speech.contains(boat)% || %speech.contains(ship)% || %speech.contains(dock)% || %speech.contains(warehouse)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say River work is slower than adventurers think and more dangerous than merchants admit. Mind ropes, current, and weather.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(road)% || %speech.contains(travel)% || %speech.contains(direction)% || %speech.contains(where)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Roads tell you where people intended to go. Couriers learn where those roads actually lead when bridges wash out and gates close.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(news)% || %speech.contains(rumor)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Couriers hear plenty. Most of it belongs to somebody else, and that is where it stays.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Good roads and fair weather to you.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3053
Zone30 Kind Soul Reactive Speech~
0 d 1
*~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
if %speech.contains(help)% || %speech.contains(lost)% || %speech.contains(new)% || %speech.contains(need)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Tell me what you are missing. If I cannot help directly, I can at least point you toward someone who can.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hurt)% || %speech.contains(wounded)% || %speech.contains(heal)% || %speech.contains(dying)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say Oh, look at you. The temple is where you need to be, and sooner rather than later.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(food)% || %speech.contains(hungry)% || %speech.contains(water)% || %speech.contains(thirst)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  say The market has food and water close by. Do not wait until you are desperate before thinking about supplies.
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
elseif %speech.contains(hello)% || %speech.contains(hail)% || %speech.contains(thank)%
  set z30_social_lock 1
  remote z30_social_lock %actor.id%
  switch %random.3%
    case 1
      say There you are. Taking care of yourself, I hope?
    break
    case 2
      emote smiles warmly at %actor.name%.
    break
    case 3
      say You are very welcome, dear.
    break
  done
  wait 3 sec
  rdelete z30_social_lock %actor.id%
  halt
end
~
#3054
Zone30 Civilian Courtesy Act~
0 e 1
waves bows nods smiles~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
set z30_social_lock 1
remote z30_social_lock %actor.id%
switch %random.4%
  case 1
    emote smiles briefly at %actor.name%.
  break
  case 2
    emote returns %actor.name%'s nod.
  break
  case 3
    say Good day to you.
  break
  case 4
    emote acknowledges %actor.name% before returning to work.
  break
done
wait 3 sec
rdelete z30_social_lock %actor.id%
~
#3055
Zone30 Civilian Distress Act~
0 e 1
cries sobs collapses faints groans staggers~
if %self.fighting%
  halt
end
if !%actor.is_pc%
  halt
end
if %actor.varexists(z30_social_lock)%
  halt
end
set z30_social_lock 1
remote z30_social_lock %actor.id%
switch %random.4%
  case 1
    say Are you all right?
  break
  case 2
    emote pauses and watches %actor.name% with concern.
  break
  case 3
    say If you are hurt, the temple is the safest place to go.
  break
  case 4
    emote looks around as though deciding whether to call for help.
  break
done
wait 3 sec
rdelete z30_social_lock %actor.id%
~
#3099
RETIRED Test Trigger~
2 b 1
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
$~
