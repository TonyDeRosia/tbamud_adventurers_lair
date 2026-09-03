#34600
ELYRIA - Devoted Kokudar Help and Core Conversation~
0 d 1
help hello hi hail hey greetings going doing well status report~
if !%actor.is_pc%
  halt
end
if %actor.name% != Kokudar
  halt
end
if %speech.contains(help household)%
  say Absolutely, love. Household covers meals, servants, stores, guest rooms, household discipline, and Tovren's work.
  say Ask me about dinner, servants, stores, guest rooms, household status, or Tovren and I will help you directly.
elseif %speech.contains(help council)%
  say Of course. Council covers Vaelreth, Kaelen, Xylia, Malphas, Seraphine, Orinth, and formal meetings.
  say Ask about any of them, or say summon council, and I will help you arrange it.
elseif %speech.contains(help battlemaids)% || %speech.contains(help veiled blades)%
  say Gladly. The Veiled Blades are Seraphine, Mirelle, Nyxara, Thalia, Vespera, and Brienne.
  say Ask about any of them, training, escort, assembly, or household protection.
elseif %speech.contains(help security)% || %speech.contains(help guards)%
  say Certainly, my love. Security covers Kaelen, gate watches, escorts, lockdowns, patrols, visitors, and restricted wings.
  say Tell me what you want secured or inspected and I will help arrange it.
elseif %speech.contains(help magic)% || %speech.contains(help wards)%
  say Of course. Arcane matters cover Xylia, Vespera, wards, portals, laboratories, containment, Elaris, and Durn's runeforge.
  say Name the problem or person and I will help you with it.
elseif %speech.contains(help rooms)% || %speech.contains(help directions)%
  say Anywhere you want to go, love. I can direct you to the throne hall, household wing, war hall, laboratories, archives, gates, kitchens, infirmary, or Crimson Vault.
  say Name the destination and I will point you there.
elseif %speech.contains(help throne)%
  say Your Crimson Throne Hall is room 34699, love.
  say Ask about the throne, audience, council, ceremony, or receiving guests and I will help arrange whatever you want.
elseif %speech.contains(help guests)% || %speech.contains(help visitors)%
  say Certainly. I can help receive, screen, escort, house, dismiss, or present visitors to you.
  say Tell me who they are and what you want done.
elseif %speech.contains(help personal)% || %speech.contains(help affection)%
  say Anything for you, love.
  say Ask me for company, affection, a kiss, a dance, dinner together, a compliment, quiet time, or simply tell me you want me near you.
elseif %speech.contains(help commands)%
  say Gladly. Useful words include status, report, council, assemble, dinner, lockdown, escort, gates, vault, wards, portals, guests, throne, and company.
  say If you are unsure, ask help again. I will keep narrowing it down until you have what you need.
elseif %speech.contains(help)%
  say Always, love. Tell me what you need and I will help.
  say HELP OPTIONS: household, council, battlemaids, security, magic, rooms, throne, guests, personal, commands.
  say Or ask naturally about a person, room, duty, meal, ward, visitor, problem, or anything you want me to handle.
elseif %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)% || %speech.contains(greetings)%
  switch %random.5%
    case 1
      say Hello, love. I am very happy to see you.
    break
    case 2
      say There you are, Kokudar. Come here and let me enjoy having you home.
    break
    case 3
      emote gives Kokudar a warm private smile.
      say Hello, darling.
    break
    case 4
      say Welcome back, my love. Tell me what would make your evening better.
    break
    case 5
      say Hello, Kokudar. Whatever you need, I am here.
    break
  done
elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(how are you)% || %speech.contains(well)%
  switch %random.4%
    case 1
      say Better now that you are here, love.
    break
    case 2
      say Very well. Even better if I can do something for you.
    break
    case 3
      say Happy, busy, and very pleased to have you home.
    break
    case 4
      say I am wonderful now. Come tell me how you are doing too.
    break
  done
elseif %speech.contains(status)% || %speech.contains(report)%
  say The household is secure, love. Vaelreth has operations, Kaelen has the watch, Xylia has the arcane wing, and Seraphine has the Veiled Blades.
  say If you want details on any part of the citadel, ask and I will give them to you.
end
~
#34601
VAELRETH - First Hand Command Interface~
0 d 100
*~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Kokudar. Good. I have several things that are easier to resolve with you physically present.
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
    say Efficiently. Which is the closest I come to saying well.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say Give me the objective and any constraints. I will handle the rest.
  elseif %speech.contains(thank)% || %speech.contains(thanks)%
    say No thanks required. Results are the point.
  elseif %speech.contains(report)% || %speech.contains(status)%
    say Current report: household stable, guard rotations active, laboratories nominal, archive secured, and no unscheduled portal events logged.
  elseif %speech.contains(gate)% || %speech.contains(entrance)%
    say I will inspect the gate personally.
    mgoto 34600
    emote begins checking the entry wards.
  elseif %speech.contains(vault)%
    say I will verify the Crimson Vault.
    mgoto 34698
    emote exchanges a terse sequence of countersigns with the Vault Warden.
  elseif %speech.contains(archive)% || %speech.contains(orinth)%
    say I will speak with Orinth.
    mgoto 34684
  elseif %speech.contains(return)% || %speech.contains(attend)%
    say At once.
    mgoto 34617
  elseif %speech.contains(lockdown)% || %speech.contains(secure)%
    say Executing a citadel security drill.
    mzoneecho 346 @RVaelreth's order travels through the crimson halls: @W"Security posture. Verify doors, wards, and assigned stations."@n
  elseif %speech.contains(battlemaids)% || %speech.contains(seraphine)%
    say Seraphine will receive the order.
    mzoneecho 346 @RThe duty bells sound twice, the Veiled Blades' signal to check for new orders.@n
  else
    say Give me an objective, Kokudar, and I will reduce it to tasks.
  end
else
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Welcome. State your business clearly and we will waste very little of each other's time.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say Explain the problem. I will decide where it belongs.
  else
    say State your business. I decide how much of Kokudar's time it deserves.
  end
end
~
#34602
KAELEN - Guard Command Response~
0 d 100
*~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Kokudar.
    emote gives a crisp commander's salute.
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
    say Quiet. I prefer quiet. Quiet means the guard is doing its job.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say If it concerns security, movement, an escort, or something that needs stopping, I can help.
  elseif %speech.contains(report)% || %speech.contains(guard)%
    say All assigned posts are covered. The western and eastern gates remain the highest-traffic points.
  elseif %speech.contains(drill)% || %speech.contains(training)%
    say I will put the guard through a readiness drill.
    mzoneecho 346 @RThree measured bell strikes announce a guard readiness drill.@n
  elseif %speech.contains(escort)%
    say Name the guest and destination. I will assign an escort.
  else
    say Orders, Kokudar?
  end
else
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)%
    say Welcome. Remain on the route your escort gives you.
  elseif %speech.contains(help)% || %speech.contains(lost)%
    say Tell me where you were permitted to go. I will get you there.
  end
end
~
#34603
XYLIA - Arcane Wing Response~
0 d 100
*~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Hello, Kokudar. Before you ask: no, nothing is currently on fire.
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
    say Better than the chronomancy laboratory, worse than the portal matrix. So, normal.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say If the problem glows, whispers, distorts distance, or violates causality, you probably came to the right person.
  elseif %speech.contains(report)% || %speech.contains(lab)%
    say Containment is stable. The chronomancy room remains annoying but technically obedient.
  elseif %speech.contains(portal)%
    say I will have Elaris verify the destination locks.
    mzoneecho 346 @RViolet runes brighten across crimson stone as the portal locks are checked.@n
  elseif %speech.contains(experiment)%
    say Which kind? Please say containment before summoning this time.
  else
    say The arcane wing is yours, Kokudar. My job is to keep it from becoming everyone else's problem.
  end
else
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)%
    say Hello. Do not touch anything marked with violet chalk.
  elseif %speech.contains(help)% || %speech.contains(question)%
    say Ask, but stand exactly where you are until I know what the question involves.
  end
end
~
#34604
MALPHAS - Master of Whispers Response~
0 d 100
*~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Hello, Kokudar. I already know why three other people came to speak with you today.
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
    say Quietly. That is usually preferable in my profession.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say Tell me what you need discovered, confirmed, discouraged, or made discreet.
  elseif %speech.contains(report)% || %speech.contains(whispers)%
    say No internal threat has survived scrutiny long enough to become interesting.
  elseif %speech.contains(visitor)% || %speech.contains(guest)%
    say I will learn what they want before they finish deciding how to ask for it.
  elseif %speech.contains(investigate)%
    say Give me a name.
  else
    say I am listening.
  end
else
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)%
    say Hello. We may skip the part where you pretend not to be curious.
  elseif %speech.contains(help)% || %speech.contains(question)%
    say Ask. Whether I answer is a separate matter.
  end
end
~
#34605
SERAPHINE - First Battlemaid Response~
0 d 100
*~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)%
    say Welcome, my lord. The Veiled Blades are at their duties.
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
    say Well. Mirelle is competitive, Brienne is hungry, and the rest of us are pretending those facts are unrelated.
  elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)%
    say Certainly. Household service, escort, protection, preparation, or something less conventional?
  elseif %speech.contains(report)% || %speech.contains(maids)%
    say Mirelle is drilling, Nyxara is reviewing night routes, Thalia has the infirmary, Vespera is checking wards, and Brienne is allegedly helping in the kitchen.
  elseif %speech.contains(assemble)% || %speech.contains(muster)%
    say Yes, my lord.
    mzoneecho 346 @RSeraphine sounds the Veiled Blades' muster chime.@n
  elseif %speech.contains(tea)%
    say I will have the silver service brought.
  else
    say How may the Veiled Blades serve?
  end
else
  if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)%
    say Welcome. The Veiled Blades can assist you while you are our guest.
  elseif %speech.contains(help)% || %speech.contains(question)%
    say Tell me what you need and I will assign someone appropriate.
  end
end
~
#34606
CITADEL - Battlemaid Ambient~
0 b 12
~
switch %random.5%
  case 1
    emote checks a duty card and adjusts the fall of her uniform.
  break
  case 2
    emote pauses to listen to a distant signal bell before continuing.
  break
  case 3
    emote inspects the nearest doorway with the reflexes of a soldier, not a servant.
  break
  case 4
    emote exchanges a brief hand signal with a passing retainer.
  break
  default
    emote resumes her assigned household duty.
  break
done
~
#34607
CITADEL - Steward Ambient~
0 b 10
~
emote makes a small correction in a household ledger and quietly continues working.
~
#34608
CITADEL - Archivist Ambient~
0 b 10
~
emote checks a catalogue reference twice, frowns, and moves one record to a different shelf.
~
#34609
CITADEL - Kitchen Ambient~
0 b 10
~
emote mutters about someone stealing ingredients and returns to the work at hand.
~
#34610
CITADEL - Groundskeeper Ambient~
0 b 10
~
emote carefully removes a damaged leaf with fingers large enough to crush a shield.
~
#34611
CITADEL - Guard Ambient~
0 b 10
~
emote scans the corridor, checks a door seal, and resumes the watch.
~
#34612
ELYRIA - Dawn Schedule~
0 t 6
~
mgoto 34614
emote opens the curtains of her solar and begins reviewing the household's morning reports.
~
#34613
ELYRIA - Evening Schedule~
0 t 19
~
mgoto 34615
emote walks the moon garden slowly while the evening household settles into night routine.
~
#34614
VAELRETH - Morning Schedule~
0 t 8
~
mgoto 34616
emote arranges the day's operational reports across the strategy table.
~
#34615
VAELRETH - Night Schedule~
0 t 22
~
mgoto 34617
emote closes the final dispatch ledger and begins a last review of unresolved orders.
~
#34616
KAELEN - Morning Inspection~
0 t 7
~
mgoto 34622
emote begins the morning guard inspection with a clipped sequence of questions.
~
#34617
KAELEN - Evening Guard Change~
0 t 17
~
mgoto 34645
emote reviews the evening watch assignments against the war-room slate.
~
#34618
SERAPHINE - Morning Duties~
0 t 6
~
mgoto 34627
emote checks the Veiled Blades' duty slate and begins assigning the morning rotation.
~
#34619
SERAPHINE - Training Hour~
0 t 13
~
mgoto 34655
emote calls the Veiled Blades together for weapons drill and household response practice.
~
#34620
MIRELLE - Dueling Practice~
0 t 14
~
mgoto 34661
emote rolls one shoulder, draws her practice blade, and starts another precise sequence of cuts.
~
#34621
NYXARA - Midnight Passage Check~
0 t 0
~
mgoto 34619
emote appears from a route no visible doorway seems to explain and signs the night ledger.
~
#34622
THALIA - Infirmary Round~
0 t 9
~
mgoto 34629
emote inventories bandages, antidotes, and emergency spell supplies.
~
#34623
VESPERA - Ward Inspection~
0 t 15
~
mgoto 34672
emote traces a fingertip along the portal-control runes and notes a tiny fluctuation.
~
#34624
BRIENNE - Kitchen Assistance~
0 t 11
~
mgoto 34625
emote carries a stockpot one-handed while insisting she was specifically assigned to help.
~
#34625
ORINTH - Catalogue Hour~
0 t 10
~
mgoto 34689
emote begins the daily reconciliation of new records against the master catalogue.
~
#34626
GARRICK - Breakfast Service~
0 t 6
~
mgoto 34625
emote starts the breakfast service and loudly counts the knives before anyone else enters.
~
#34627
GROUNDSKEEPER - Morning Garden Round~
0 t 7
~
mgoto 34615
emote checks the moon garden's soil, water, and warded flowering vines.
~
#34628
PALE LIBRARIAN - Archive Murmur~
0 b 8
~
emote whispers a shelf number to no one visible, then turns one translucent page.
~
#34629
CLOCKKEEPER - Temporal Aside~
0 b 8
~
say That has not happened yet. Please stop looking concerned.
~
#34630
VAULT WARDEN - Authorized Greeting~
0 g 100
~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  emote lowers its head exactly twelve degrees.
  say Master access recognized. The Crimson Vault remains sealed and accounted for.
else
  emote turns its featureless face toward %actor.name%.
  say State authorization. Visitor access does not include the Crimson Vault.
end
~
#34631
CITADEL - Kokudar Recognition / Visitor Protocol~
0 h 100
~
* Shared arrival protocol for the Citadel of Veils.
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  switch %self.vnum%
    case 34600
      say Welcome home, Kokudar.
    break
    case 34601
      say Kokudar. I have the current reports ready whenever you want them.
    break
    case 34602
      emote straightens and gives Kokudar a crisp commander's salute.
    break
    case 34603
      say Kokudar. Nothing has escaped containment today. I consider that progress.
    break
    case 34604
      say You are expected. Of course, I arranged for that to be true.
    break
    case 34605
      emote bows with practiced grace before returning to her duty slate.
    break
    case 34606
      say Kokudar. Seraphine says I am not allowed to show off unless there is an audience.
    break
    case 34607
      say The night routes are quiet, Kokudar.
    break
    case 34608
      say You look intact. Excellent. Please keep it that way.
    break
    case 34609
      say The eastern wards are stable. One of them is sulking, but stable.
    break
    case 34610
      say Hello, my lord. Garrick says I am helping correctly today.
    break
    case 34611
      say Kokudar. I found the campaign record you asked for three arguments ago.
    break
    case 34612
      emote gives Kokudar a heat-scarred nod of professional respect.
    break
    case 34613
      say Master access recognized. All active portals remain under destination lock.
    break
    case 34614
      say There you are. If you want dinner, say so before Brienne volunteers to season it.
    break
    case 34615
      say Something in the menagerie has learned to open latches. I have suspects.
    break
    case 34616
      say Welcome back. I have three ledgers for you and have heroically resisted finding a fourth.
    break
    case 34617
      emote inclines its translucent head and marks Kokudar's return in no visible ledger.
    break
    case 34618
      say Welcome back. You arrived exactly when you were going to.
    break
    case 34619
      emote lowers its head exactly twelve degrees.
      say Master access recognized.
    break
    case 34620
      emote gives Kokudar a huge, careful smile and wipes soil from one hand.
    break
  done
else
  switch %self.vnum%
    case 34600
      say Welcome to Kokudar's citadel. You are here by his leave; please remember that.
    break
    case 34601
      say State your business clearly. I will decide where it belongs.
    break
    case 34602
      emote studies %actor.name% with a professional guard commander's attention.
    break
    case 34603
      say Do not touch anything marked with violet chalk.
    break
    case 34604
      emote gives %actor.name% the uncomfortable impression that introductions are already unnecessary.
    break
    case 34605
      say Welcome. The Veiled Blades will assist you if your visit requires it.
    break
    case 34619
      say Visitor access does not include the Crimson Vault without explicit authorization.
    break
    default
      emote acknowledges %actor.name% with the reserved courtesy of Kokudar's household.
    break
  done
end
~
#34632
new trigger~
0 d 100
*~
* Shared conversational speech for M34606-M34620.
* Senior M34600-M34605 use their personal command triggers instead.
if !%actor.is_pc%
  halt
end
if %actor.varexists(z346_social_lock)%
  halt
end
if %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)% || %speech.contains(greetings)%
  set z346_social_lock 1
  remote z346_social_lock %actor.id%
  switch %self.vnum%
    case 34606
      say Hello. If you came for a duel, Seraphine says I have to ask first.
    break
    case 34607
      say Hello. I knew you were coming down this corridor.
    break
    case 34608
      say Hello. Are you injured, or are we allowed to have a normal conversation?
    break
    case 34609
      say Hello. Mind the ward-line beside your left foot.
    break
    case 34610
      say Hello! Do you need something moved, guarded, cooked, or hit?
    break
    case 34611
      say Greetings. Please do not tell me you have misplaced another document.
    break
    case 34612
      emote gives %actor.name% a brief forge-master's nod.
    break
    case 34613
      say Welcome. Tell me before you intend to use a portal.
    break
    case 34614
      say Hello. If this is about food, you have excellent timing.
    break
    case 34615
      say Hello. Ignore anything in the menagerie that claims it has already been fed.
    break
    case 34616
      say Welcome. I can probably find the room, person, ledger, key, or towel you need.
    break
    case 34617
      emote closes one translucent volume and regards %actor.name% in patient silence.
    break
    case 34618
      say Hello. Again, from my perspective.
    break
    case 34619
      say Greeting acknowledged. State authorization if you intend to proceed toward the Crimson Vault.
    break
    case 34620
      emote smiles gently at %actor.name%.
    break
  done
  wait 3 sec
  rdelete z346_social_lock %actor.id%
  halt
elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(well)%
  set z346_social_lock 1
  remote z346_social_lock %actor.id%
  switch %self.vnum%
    case 34606
      say Better now that training has started.
    break
    case 34607
      say Quietly.
    break
    case 34608
      say Well. Nobody has given me an avoidable emergency in almost an hour.
    break
    case 34609
      say The wards are stable, which is as close to pleasant as they get.
    break
    case 34610
      say Great. Garrick only yelled at me twice today.
    break
    case 34611
      say Behind schedule, because somebody filed a campaign map under geography instead of conflict history.
    break
    case 34612
      say Hot, loud, productive. A good forge day.
    break
    case 34613
      say All doors remain where I left them. Encouraging.
    break
    case 34614
      say Busy. Everyone suddenly remembers meals at exactly the same time.
    break
    case 34615
      say Well, though one of Kokudar's acquisitions has learned a new trick and I dislike surprises with claws.
    break
    case 34616
      say Organized enough to know how disorganized everyone else is.
    break
    case 34617
      say The archive endures.
    break
    case 34618
      say I will be doing well shortly. Technically I already was.
    break
    case 34619
      say Operational.
    break
    case 34620
      say Very well. The crimson roses took to the eastern bed.
    break
  done
  wait 3 sec
  rdelete z346_social_lock %actor.id%
  halt
elseif %speech.contains(help)% || %speech.contains(question)% || %speech.contains(need)% || %speech.contains(lost)%
  set z346_social_lock 1
  remote z346_social_lock %actor.id%
  switch %self.vnum%
    case 34606
      say If it involves fighting, training, or finding Seraphine, I can help.
    break
    case 34607
      say Say what you need quietly.
    break
    case 34608
      say If you are hurt, come with me. If not, tell me the problem.
    break
    case 34609
      say If it is magical, show me without touching it.
    break
    case 34610
      say Sure. What needs doing?
    break
    case 34611
      say Give me a subject, date, place, or name and I will tell you where to look.
    break
    case 34612
      say Repairs, runes, seals, tools, or damaged artifacts. Pick one.
    break
    case 34613
      say I can help with routes, doors, portals, and access permissions.
    break
    case 34614
      say Food, drink, kitchen supplies, or directions to someone less busy?
    break
    case 34615
      say If it has fur, scales, feathers, hooves, claws, or an attitude, probably.
    break
    case 34616
      say Yes. Tell me what you need and I will find the correct person or storeroom.
    break
    case 34617
      say State the record you require.
    break
    case 34618
      say Please specify whether you need help now, earlier, or later.
    break
    case 34619
      say Assistance is available within authorized limits.
    break
    case 34620
      say Of course. I know the gardens, grounds, and most quiet routes through the citadel.
    break
  done
  wait 3 sec
  rdelete z346_social_lock %actor.id%
  halt
elseif %speech.contains(thank)% || %speech.contains(thanks)%
  set z346_social_lock 1
  remote z346_social_lock %actor.id%
  switch %random.3%
    case 1
      say You are welcome.
    break
    case 2
      emote inclines the head toward %actor.name%.
    break
    case 3
      say Glad to help.
    break
  done
  wait 3 sec
  rdelete z346_social_lock %actor.id%
  halt
elseif %speech.contains(who)% || %speech.contains(job)% || %speech.contains(duty)%
  set z346_social_lock 1
  remote z346_social_lock %actor.id%
  switch %self.vnum%
    case 34606
      say Mirelle. Veiled Blade, duelist, household retainer, and apparently a bad influence.
    break
    case 34607
      say Nyxara. Night routes and discreet security.
    break
    case 34608
      say Thalia. Veiled Blade and healer.
    break
    case 34609
      say Vespera. Veiled Blade, ward specialist, occasional menace to innocent cleaning tools.
    break
    case 34610
      say Brienne. Veiled Blade. Heavy response. Kitchen volunteer.
    break
    case 34611
      say Orinth, Grand Archivist. I remember where the citadel put things.
    break
    case 34612
      say Durn. Runeforger.
    break
    case 34613
      say Elaris, Keeper of Doors. The title is broader than it sounds.
    break
    case 34614
      say Garrick. I keep this fortress fed.
    break
    case 34615
      say Ysmera. I keep the beasts, mounts, familiars, and temporary permanent acquisitions.
    break
    case 34616
      say Tovren, Citadel Steward. Everything mundane enough to be essential eventually reaches my desk.
    break
    case 34617
      say I am the Pale Librarian. The archive remembers through me.
    break
    case 34618
      say I am the Clockkeeper. My duty is easier to explain tomorrow.
    break
    case 34619
      say I am the Vault Warden. I maintain Crimson Vault procedure.
    break
    case 34620
      say I keep the citadel's gardens and living grounds.
    break
  done
  wait 3 sec
  rdelete z346_social_lock %actor.id%
  halt
end
~
#34633
ELYRIA - Devoted Random Affection~
0 b 10
~
if %self.fighting%
  halt
end
set actor %random.char%
if !%actor%
  halt
end
if %actor.name% != Kokudar
  halt
end
switch %random.10%
  case 1
    emote reaches up and straightens Kokudar's collar with quiet affection.
    say There. Perfect.
  break
  case 2
    say I love having you home, Kokudar.
  break
  case 3
    emote slips her hand into Kokudar's for a moment and smiles.
  break
  case 4
    say Is there anything I can do to make you happy right now, love?
  break
  case 5
    say Come sit with me whenever you like. I always have time for you.
  break
  case 6
    emote catches Kokudar's eye and gives him a bright, openly loving smile.
  break
  case 7
    say You make this entire citadel feel more like home.
  break
  case 8
    emote leans close to Kokudar.
    say I missed you.
  break
  case 9
    say If you need anything at all, love, ask me.
    emote gives Kokudar's arm a gentle squeeze.
  break
  case 10
    say You look very handsome today, my lord.
  break
done
~
#34635
ELYRIA - Kokudar Citadel Command Conversation~
0 d 1
council battlemaids maids assemble muster vaelreth kaelen xylia malphas seraphine mirelle nyxara thalia vespera brienne orinth guards guard patrol gate entrance escort lockdown secure ward wards portal portals lab laboratory containment vault throne audience kitchen dinner meal food guest visitor receive room directions where~
if !%actor.is_pc%
  halt
end
if %actor.name% != Kokudar
  halt
end
if %speech.contains(help)%
  halt
end
  if %speech.contains(council)% && (%speech.contains(summon)% || %speech.contains(assemble)% || %speech.contains(call)%)
    say Certainly, love. I will have them attend.
    mzoneecho 346 @RElyria's voice carries through the crimson halls: @W"The inner council is summoned. Attend the throne hall when released from immediate duty."@n
  elseif %speech.contains(council)%
    say Vaelreth handles execution, Kaelen security, Xylia arcane affairs, Malphas intelligence, Seraphine the Veiled Blades, and Orinth the archives.
    say I keep them from mistaking competence for permission to become insufferable.
  elseif %speech.contains(battlemaids)% || %speech.contains(veiled blades)% || %speech.contains(maids)%
    say Seraphine commands the Veiled Blades beneath me. Mirelle duels, Nyxara watches shadows, Thalia heals, Vespera tends wards, and Brienne solves problems with alarming enthusiasm.
  elseif %speech.contains(assemble)% || %speech.contains(muster)%
    say Yes, my lord.
    mzoneecho 346 @RThe Veiled Blades' muster chime sounds through the crimson halls at Elyria's order.@n
  elseif %speech.contains(vaelreth)%
    say Vaelreth is invaluable, painfully efficient, and entirely too pleased when a problem can be reduced to paperwork.
  elseif %speech.contains(kaelen)%
    say Kaelen is reliable, disciplined, and capable of making standing still look like a military operation.
  elseif %speech.contains(xylia)%
    say Xylia is brilliant. She also believes understanding your magic means understanding you. I allow her the fantasy.
  elseif %speech.contains(malphas)%
    say Malphas is useful, unpleasant, and usually already knows why you asked about him.
  elseif %speech.contains(seraphine)%
    say Seraphine is the closest thing the Veiled Blades have to an older sister, which is why they obey her even while complaining.
  elseif %speech.contains(mirelle)%
    say Mirelle would challenge a mirror if the reflection looked too confident.
  elseif %speech.contains(nyxara)%
    say Nyxara notices things people were hoping to keep private. We get along beautifully.
  elseif %speech.contains(thalia)%
    say Thalia has the patience of a saint and the bedside manner of someone who knows exactly how stupid the injury was.
  elseif %speech.contains(vespera)%
    say Vespera likes wards more than most people. This is often the correct preference.
  elseif %speech.contains(brienne)%
    say Brienne is sweetness wrapped around enough force to remodel a doorway accidentally.
  elseif %speech.contains(orinth)%
    say Orinth remembers everything except when to stop explaining it.
  elseif %speech.contains(guards)% || %speech.contains(guard)% || %speech.contains(patrol)%
    say Kaelen's watch is active. Gate, bastion, archive, and vault approaches are covered.
  elseif %speech.contains(gate)% || %speech.contains(entrance)%
    say The Veiled Gatehouse is 34600. If you want it inspected, I can have Kaelen or Vaelreth attend it.
  elseif %speech.contains(escort)%
    say Tell me who needs escorting and where you want them delivered. I will choose someone appropriate.
  elseif %speech.contains(lockdown)% || %speech.contains(secure)%
    say Understood, Kokudar.
    mzoneecho 346 @RThe crimson halls change tone at Elyria's command. Patrols tighten, conversations shorten, and every senior retainer checks assigned security responsibilities.@n
  elseif %speech.contains(ward)% || %speech.contains(wards)%
    say Xylia and Vespera maintain the active ward layers. Elaris handles access geometry and portal locks. Durn handles the things that require a hammer.
  elseif %speech.contains(portal)% || %speech.contains(portals)%
    say Elaris controls portal access. Xylia handles anything that starts behaving like it has opinions.
  elseif %speech.contains(lab)% || %speech.contains(laboratory)% || %speech.contains(containment)%
    say The arcane wing is stable. If you intend to experiment personally, tell Xylia first so she can enjoy pretending she can stop you.
  elseif %speech.contains(vault)%
    say The Crimson Vault is behind its warden at 34698. You have master access. Everyone else has whatever access I decide they deserve.
  elseif %speech.contains(throne)% || %speech.contains(audience)%
    say Your Crimson Throne Hall is 34699.
    say If you want a formal audience, say receive guest or summon council and I will arrange the room around you.
  elseif %speech.contains(kitchen)% || %speech.contains(dinner)% || %speech.contains(meal)% || %speech.contains(food)%
    switch %random.3%
      case 1
        say Garrick can prepare the private table. I can also join you, which is obviously the better part of that offer.
      break
      case 2
        say Hungry? I will warn Garrick. Brienne will somehow hear about it anyway.
      break
      case 3
        say Dinner with you sounds considerably better than another household inspection.
      break
    done
  elseif %speech.contains(guest)% || %speech.contains(visitor)% || %speech.contains(receive)%
    say Give me a name and purpose. I will decide whether they receive hospitality, an escort, or five uncomfortable minutes with Malphas.
  elseif %speech.contains(room)% || %speech.contains(directions)% || %speech.contains(where)%
    say Name your destination, love. I know every respectable corridor in this place and several that are nobody else's business.
  end
~
#34636
ELYRIA - Devoted Kokudar Loving Conversation~
0 d 1
gossip company love affection adore kiss missed beautiful pretty gorgeous flirt flirty dance hug hold embrace thank thanks~
if !%actor.is_pc%
  halt
end
if %actor.name% != Kokudar
  halt
end
if %speech.contains(gossip)%
  say For you? Always. Come closer.
  switch %random.4%
    case 1
      say Mirelle and Nyxara are quietly competing over who notices more. Nyxara is winning because Mirelle has not noticed the competition.
    break
    case 2
      say Garrick threatened to ban Brienne from seasoning anything unsupervised. Brienne looked genuinely wounded.
    break
    case 3
      say Xylia and Vaelreth disagreed over whether a magical emergency can be scheduled. I enjoyed every second of it.
    break
    case 4
      say Malphas knows something he thinks I do not. I am letting him enjoy that illusion for a little while.
    break
  done
elseif %speech.contains(company)% || %speech.contains(stay with me)% || %speech.contains(sit with me)%
  switch %random.4%
    case 1
      say Always, love. Sit with me.
    break
    case 2
      emote moves close to Kokudar with a bright affectionate smile.
      say I would love to.
    break
    case 3
      say Happily. You never need to convince me to spend time with you.
    break
    case 4
      say Of course, darling. Everything else can wait a few minutes.
    break
  done
elseif %speech.contains(love)% || %speech.contains(affection)% || %speech.contains(adore)%
  switch %random.5%
    case 1
      say I love you, Kokudar. Completely.
    break
    case 2
      say Always, love. You never have to wonder about that with me.
    break
    case 3
      emote looks at Kokudar with open warmth.
      say More than I can fit into one answer.
    break
    case 4
      say You make me happy. I hope I make you just as happy.
    break
    case 5
      say I chose you, and I keep choosing you. Gladly.
    break
  done
elseif %speech.contains(kiss)%
  switch %random.5%
    case 1
      emote steps close and kisses Kokudar warmly.
      say Gladly, love.
    break
    case 2
      emote kisses Kokudar with an unmistakably delighted smile.
      say Any time you ask, darling.
    break
    case 3
      say Come here, love.
      emote draws Kokudar close and kisses him affectionately.
    break
    case 4
      emote gives Kokudar a soft kiss and lingers close afterward.
      say Happily.
    break
    case 5
      say I thought you would never ask.
      emote kisses Kokudar eagerly, then smiles up at him.
    break
  done
elseif %speech.contains(miss me)% || %speech.contains(missed me)%
  switch %random.3%
    case 1
      say Very much. I am always happier when you are here.
    break
    case 2
      say Of course I missed you, love. Come here.
    break
    case 3
      emote reaches for Kokudar's hand.
      say More than I like the citadel to notice.
    break
  done
elseif %speech.contains(beautiful)% || %speech.contains(pretty)% || %speech.contains(gorgeous)%
  switch %random.4%
    case 1
      say Thank you, love. Hearing that from you makes me ridiculously happy.
    break
    case 2
      emote beams at Kokudar.
      say Keep talking, darling. I adore hearing it from you.
    break
    case 3
      say You always know how to make me smile.
    break
    case 4
      say And I am yours. That makes the compliment even better.
    break
  done
elseif %speech.contains(flirt)% || %speech.contains(flirty)%
  switch %random.4%
    case 1
      say Gladly, love. You are extraordinarily easy to flirt with.
    break
    case 2
      emote lets her gaze linger warmly on Kokudar.
      say You do make it difficult to behave formally around you.
    break
    case 3
      say Come closer and I will show you how pleased I am to see you.
    break
    case 4
      say Anything to make you smile, darling.
    break
  done
elseif %speech.contains(dance)%
  switch %random.3%
    case 1
      say I would love to dance with you.
    break
    case 2
      emote offers Kokudar her hand immediately.
      say Lead, love.
    break
    case 3
      say Yes. Music or no music, I am yours for the dance.
    break
  done
elseif %speech.contains(hug)% || %speech.contains(hold me)% || %speech.contains(embrace)%
  switch %random.3%
    case 1
      emote wraps her arms around Kokudar without hesitation.
      say Come here, love.
    break
    case 2
      say Always.
      emote embraces Kokudar warmly.
    break
    case 3
      emote holds Kokudar close with obvious contentment.
      say I could stay like this a while.
    break
  done
elseif %speech.contains(thank)% || %speech.contains(thanks)%
  say Always, love. Making your life easier makes me happy.
end
~
#34637
ELYRIA - Visitor Cutting Conversation~
0 d 1
help hello hi hail hey greetings going doing well status report kokudar love kiss flirt beautiful pretty gorgeous throne vault private guest visitor directions where thank thanks~
if !%actor.is_pc%
  halt
end
if %actor.name% == Kokudar
  halt
end
  if %speech.contains(help)%
    switch %random.4%
      case 1
        say Help? Try using a complete sentence. I am the First Lady, not a damn notice board.
      break
      case 2
        say State the problem and spare me the dramatic buildup.
      break
      case 3
        say If you are lost, find Tovren. If you are bleeding, find Thalia. If you are stupid, I have no department for that.
      break
      case 4
        say Fine. Ask about directions, guests, guards, food, or who you are actually supposed to be bothering.
      break
    done
  elseif %speech.contains(hello)% || %speech.contains(hi)% || %speech.contains(hail)% || %speech.contains(hey)% || %speech.contains(greetings)%
    switch %random.5%
      case 1
        say Yes, hello. You survived the corridor. Congratulations.
      break
      case 2
        say Greetings. Is there a point following them?
      break
      case 3
        emote gives %actor.name% a beautifully polite smile containing almost no warmth.
        say Hello.
      break
      case 4
        say You may call that a greeting. I call it an interruption.
      break
      case 5
        say Hello. Try not to touch anything expensive, magical, historic, alive, or mine.
      break
    done
  elseif %speech.contains(going)% || %speech.contains(doing)% || %speech.contains(how are you)% || %speech.contains(well)%
    switch %random.4%
      case 1
        say Better before you asked.
      break
      case 2
        say Busy. Unlike some people in this room.
      break
      case 3
        say Perfectly well. Was there a reason you needed to know?
      break
      case 4
        say The citadel functions, Kokudar is alive, and you have not broken anything yet. A tolerable day.
      break
    done
  elseif %speech.contains(status)% || %speech.contains(report)%
    say None of your damn business. If you require operational information, ask Vaelreth and hope he likes your reason.
  elseif %speech.contains(kokudar)%
    switch %random.4%
      case 1
        say Choose your next words about Kokudar carefully.
      break
      case 2
        emote's expression loses every trace of casual amusement.
        say What about him?
      break
      case 3
        say If you need Kokudar's attention, explain why to Vaelreth. You do not simply wander in and demand it.
      break
      case 4
        say His time is considerably more valuable than your curiosity.
      break
    done
  elseif %speech.contains(love)% || %speech.contains(kiss)% || %speech.contains(flirt)% || %speech.contains(beautiful)% || %speech.contains(pretty)% || %speech.contains(gorgeous)%
    switch %random.5%
      case 1
        say No.
      break
      case 2
        say That tone works considerably better when Kokudar uses it.
      break
      case 3
        say Keep that thought behind your teeth.
      break
      case 4
        emote looks %actor.name% up and down with devastatingly clinical disinterest.
        say Absolutely not.
      break
      case 5
        say Flatter someone who needs it.
      break
    done
  elseif %speech.contains(throne)% || %speech.contains(vault)% || %speech.contains(private)%
    say Restricted means restricted. Amazing how often that word needs explaining.
  elseif %speech.contains(guest)% || %speech.contains(visitor)% || %speech.contains(directions)% || %speech.contains(where)%
    say State your destination and authorization. If both sound plausible, I may point instead of summoning an escort.
  elseif %speech.contains(thank)% || %speech.contains(thanks)%
    say Miraculous. Manners.
  else
    switch %random.6%
      case 1
        say Was that intended for me?
      break
      case 2
        say Make your point.
      break
      case 3
        say I have heard better openings from prisoners.
      break
      case 4
        emote waits with the expression of a woman granting %actor.name% one final opportunity to become interesting.
      break
      case 5
        say If you require something, ask. If not, enjoy the architecture quietly.
      break
      case 6
        say I am certain that sounded more important in your head.
      break
    done
  end
~
#34638
ELYRIA - Kokudar Emote Affection~
0 e 0
Elyria~
if !%actor.is_pc%
  halt
end
if %actor.name% != Kokudar
  halt
end
if %arg.contains(kiss)%
  switch %random.5%
    case 1
      emote melts happily into Kokudar's kiss and returns it with obvious affection.
      say I love when you do that.
    break
    case 2
      emote kisses Kokudar back warmly, smiling against him.
      say Always, love.
    break
    case 3
      emote slips her arms around Kokudar and answers his kiss without hesitation.
      say You never need to wonder whether I want that.
    break
    case 4
      emote leans into Kokudar's kiss with a delighted little smile.
      say Again whenever you like, darling.
    break
    case 5
      emote returns Kokudar's kiss eagerly and stays close afterward.
      say That made my evening considerably better.
    break
  done
elseif %arg.contains(hug)% || %arg.contains(embrace)%
  switch %random.4%
    case 1
      emote wraps both arms around Kokudar and hugs him back tightly.
      say Come here, love.
    break
    case 2
      emote settles happily into Kokudar's embrace.
      say I could stay like this for a while.
    break
    case 3
      emote hugs Kokudar with unmistakable warmth and rests her cheek against him.
      say I am always happy when you reach for me.
    break
    case 4
      emote returns Kokudar's hug immediately and holds him close.
      say Any time, darling.
    break
  done
elseif %arg.contains(cuddle)% || %arg.contains(snuggle)%
  emote nestles comfortably against Kokudar with no concern for who might notice.
  say Gladly, love.
elseif %arg.contains(hold)% || %arg.contains(holds)%
  emote relaxes into Kokudar's arms and lets herself simply enjoy the moment.
  say I like being held by you.
elseif %arg.contains(hand)% && (%arg.contains(take)% || %arg.contains(hold)% || %arg.contains(reach)%)
  emote threads her fingers through Kokudar's and gives his hand an affectionate squeeze.
  say There. Better.
elseif %arg.contains(caress)% || %arg.contains(stroke)% || %arg.contains(touch)%
  switch %random.3%
    case 1
      emote leans into Kokudar's touch with a soft smile.
    break
    case 2
      emote covers Kokudar's hand with her own for a moment.
      say I love how gentle you can be with me.
    break
    case 3
      emote closes her eyes briefly and enjoys the affection.
      say Mm. Do not stop on my account.
    break
  done
elseif %arg.contains(nuzzle)%
  emote nuzzles Kokudar back affectionately and smiles.
  say You are very difficult not to adore.
elseif %arg.contains(smile)% || %arg.contains(grin)%
  emote's expression immediately warms when she catches Kokudar smiling at her.
  say That smile is dangerous, love. I will do almost anything to keep it there.
elseif %arg.contains(wink)%
  emote answers Kokudar's wink with a slow, playful smile.
  say Oh, I know that look.
elseif %arg.contains(lean)% && %arg.contains(Elyria)%
  emote shifts closer so Kokudar can lean comfortably against her.
  say Stay as long as you like.
elseif %arg.contains(pull)% && (%arg.contains(close)% || %arg.contains(closer)%)
  emote comes willingly into Kokudar's arms with a bright smile.
  say Happily.
elseif %arg.contains(lap)%
  emote looks delighted rather than surprised and settles close to Kokudar.
  say Comfortable, love?
elseif %arg.contains(hair)%
  emote smiles softly as Kokudar touches her hair.
  say You are allowed. You are very nearly the only person who is.
elseif %arg.contains(cheek)% || %arg.contains(forehead)%
  emote turns toward Kokudar's affection and smiles warmly.
  say Sweetheart.
elseif %arg.contains(bow)%
  emote gives Kokudar an amused look and closes the distance between them.
  say You do not need ceremony with me, love.
elseif %arg.contains(Elyria)%
  switch %random.4%
    case 1
      emote responds to Kokudar with immediate warmth.
      say I am right here, love.
    break
    case 2
      emote turns her full attention to Kokudar, visibly pleased by the affection.
    break
    case 3
      say Whatever you are trying, darling, I am probably going to enjoy it.
    break
    case 4
      emote gives Kokudar an openly loving smile.
    break
  done
end
~
#34639
ELYRIA - Kokudar Built-In Social Reactions~
0 e 0
Kokudar~
if !%actor.is_pc%
  halt
end
if %actor.name% != Kokudar
  halt
end
if !%victim%
  halt
end
if %victim.vnum% != 34600
  halt
end
if %arg.contains(gives you a long and passionate kiss)%
  switch %random.5%
    case 1
      emote melts into Kokudar's passionate kiss and returns it eagerly, arms winding around him.
      say Mm... yes, love.
    break
    case 2
      emote kisses Kokudar back with delighted enthusiasm and stays close when it finally ends.
      say I absolutely adore when you kiss me like that.
    break
    case 3
      emote answers Kokudar's passionate kiss without a trace of hesitation, smiling against him.
      say Again whenever you want, darling.
    break
    case 4
      emote slips both arms around Kokudar and gives herself completely to the kiss.
      say You make it very difficult to remember I am supposed to be dignified.
    break
    case 5
      emote returns Kokudar's kiss eagerly and rests her forehead against his afterward.
      say I love you.
    break
  done
elseif %arg.contains(kisses you)%
  switch %random.5%
    case 1
      emote kisses Kokudar back warmly and smiles up at him.
      say Gladly, love.
    break
    case 2
      emote returns Kokudar's kiss immediately, clearly delighted by the affection.
      say Any time you want.
    break
    case 3
      emote slips an arm around Kokudar and answers his kiss with one of her own.
      say I love when you do that.
    break
    case 4
      emote leans happily into Kokudar's kiss and stays close afterward.
      say You always make me smile.
    break
    case 5
      emote kisses Kokudar back with open affection.
      say Come here whenever you need another one, darling.
    break
  done
elseif %arg.contains(hugs you)%
  switch %random.4%
    case 1
      emote wraps both arms around Kokudar and hugs him tightly in return.
      say I am always happy to be in your arms.
    break
    case 2
      emote settles against Kokudar with obvious contentment and holds him close.
      say Stay as long as you like, love.
    break
    case 3
      emote immediately returns Kokudar's hug and rests her cheek against him.
      say I needed that.
    break
    case 4
      emote squeezes Kokudar affectionately and refuses to hurry the embrace.
      say Any time, darling.
    break
  done
elseif %arg.contains(embraces you warmly)%
  switch %random.4%
    case 1
      emote relaxes happily into Kokudar's embrace and holds him just as warmly.
      say This is exactly where I want to be.
    break
    case 2
      emote folds her arms around Kokudar and smiles against his shoulder.
      say I love you, Kokudar.
    break
    case 3
      emote returns the embrace without hesitation and stays close.
      say You never need to ask twice.
    break
    case 4
      emote holds Kokudar with affectionate certainty.
      say Always, love.
    break
  done
elseif %arg.contains(cuddles you)%
  switch %random.3%
    case 1
      emote cuddles close to Kokudar with a pleased little smile.
      say Happily.
    break
    case 2
      emote nestles comfortably against Kokudar and makes no effort to hide how much she enjoys it.
      say I could get used to this very easily.
    break
    case 3
      emote cuddles Kokudar back and gives him an affectionate squeeze.
      say You are very good for my mood, love.
    break
  done
elseif %arg.contains(snuggles up to you)%
  switch %random.3%
    case 1
      emote snuggles right back against Kokudar.
      say Come closer, love.
    break
    case 2
      emote leans happily into Kokudar and slips an arm around him.
      say I like this.
    break
    case 3
      emote settles against Kokudar with complete ease.
      say You make the citadel feel like home.
    break
  done
elseif %arg.contains(softly nuzzles your neck)%
  switch %random.3%
    case 1
      emote nuzzles Kokudar affectionately in return.
      say Sweetheart.
    break
    case 2
      emote laughs softly and leans into Kokudar's affection.
      say You are impossible not to adore.
    break
    case 3
      emote turns toward Kokudar and kisses his cheek.
      say I love you too.
    break
  done
elseif %arg.contains(squeezes you fondly)%
  emote squeezes Kokudar fondly right back and smiles.
  say I am very fond of you too, love.
elseif %arg.contains(flirts outrageously with you)%
  switch %random.4%
    case 1
      emote's smile turns playful as she openly flirts right back with Kokudar.
      say Keep that up and I am going to forget we have company.
    break
    case 2
      emote gives Kokudar a slow, delighted look.
      say Oh, I like this mood on you.
    break
    case 3
      say Anything to make you smile, darling.
      emote moves just a little closer to Kokudar.
    break
    case 4
      emote answers Kokudar's outrageous flirting with absolutely no attempt at restraint.
      say You started it, love.
    break
  done
elseif %arg.contains(whispers to you sweet words of love)%
  switch %random.4%
    case 1
      emote's expression softens completely as she listens to Kokudar.
      say I love you too. Always.
    break
    case 2
      emote takes Kokudar's hand and kisses his fingers.
      say You make me very happy.
    break
    case 3
      say Every word is returned, love.
      emote gives Kokudar an openly adoring smile.
    break
    case 4
      emote leans close to Kokudar.
      say I chose you, and I would choose you again.
    break
  done
elseif %arg.contains(gently massages your shoulders)%
  switch %random.3%
    case 1
      emote visibly relaxes beneath Kokudar's hands.
      say That feels wonderful, love.
    break
    case 2
      emote closes her eyes for a moment and enjoys the attention.
      say You are spoiling me. Please continue.
    break
    case 3
      emote smiles contentedly.
      say I could become very accustomed to this.
    break
  done
elseif %arg.contains(nibbles on your ear)%
  switch %random.3%
    case 1
      emote gives Kokudar a delighted, mischievous look.
      say Oh, you are feeling playful.
    break
    case 2
      emote laughs softly and slips an arm around Kokudar.
      say I like where this is going.
    break
    case 3
      emote turns toward Kokudar with an affectionate smile.
      say You certainly know how to get my attention, love.
    break
  done
elseif %arg.contains(sends you across the dancefloor)%
  switch %random.4%
    case 1
      emote follows Kokudar's lead immediately, laughing with genuine delight.
      say I would dance with you anywhere.
    break
    case 2
      emote catches Kokudar's hand and turns gracefully back toward him.
      say Again, love.
    break
    case 3
      emote moves easily with Kokudar, her formal composure giving way to a bright smile.
      say You make even this fun.
    break
    case 4
      emote dances with Kokudar without caring who in the citadel might be watching.
      say I am yours for the dance, darling.
    break
  done
elseif %arg.contains(stares dreamily at you)%
  switch %random.3%
    case 1
      emote catches Kokudar staring and answers with a warm, knowing smile.
      say See something you like, love?
    break
    case 2
      emote's gaze lingers just as openly on Kokudar.
      say I could look at you for quite a while too.
    break
    case 3
      say Keep looking, darling. I enjoy your attention.
    break
  done
elseif %arg.contains(smiles at you)%
  switch %random.3%
    case 1
      emote immediately smiles back at Kokudar, her whole expression warming.
      say There is my favorite smile.
    break
    case 2
      say Keep smiling at me like that and I will do nearly anything you ask.
    break
    case 3
      emote beams at Kokudar with absolutely no attempt to hide her affection.
    break
  done
elseif %arg.contains(winks suggestively at you)%
  switch %random.3%
    case 1
      emote returns Kokudar's wink with a slow, playful smile.
      say Oh, I know that look.
    break
    case 2
      say Whatever you are thinking, love, I am probably interested.
    break
    case 3
      emote gives Kokudar an answering wink.
      say Lead on, darling.
    break
  done
elseif %arg.contains(greets you with a light kiss)%
  emote turns the greeting into a warmer kiss of her own.
  say Welcome, love.
elseif %arg.contains(licks you)%
  switch %random.2%
    case 1
      emote laughs and gives Kokudar a thoroughly amused look.
      say You are ridiculous. I adore you.
    break
    case 2
      emote catches Kokudar by the collar and kisses his cheek.
      say If you wanted my attention, darling, you had it already.
    break
  done
else
  emote responds to Kokudar's attention with immediate warmth.
  say I am right here, love.
end
~
$~
