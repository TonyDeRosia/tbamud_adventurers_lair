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
#3099
RETIRED Test Trigger~
2 b 1
~
* RETIRED legacy stock trigger. Intentionally inert in Adventurer's Lair.
~
$~
