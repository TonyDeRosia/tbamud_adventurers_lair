#18670
DG Foundation - Cooldown Test~
0 d 1
dgtest~
if %actor.is_pc%
  dg_cooldown_check %actor% foundation_test
  msend %actor% CHECK1=%dg_cooldown_result%
  dg_cooldown_set %actor% foundation_test 10
  msend %actor% SET=%dg_cooldown_result%
  dg_cooldown_check %actor% foundation_test
  msend %actor% CHECK2=%dg_cooldown_result%
end
~
#18671
DG Foundation - Kick Test~
0 k 100
~
if %actor.is_pc%
  dg_cooldown_check %actor% foundation_kick
  msend %actor% KICKCHECK=%dg_cooldown_result%
  if %dg_cooldown_result% == READY
    dg_skill kick %actor%
    msend %actor% SKILL=%dg_skill_result%
    if %dg_skill_result% == ATTEMPTED
      dg_cooldown_set %actor% foundation_kick 8
      msend %actor% KICKSET=%dg_cooldown_result%
    end
  end
end
~
#18680
Academia - Elowen Orientation~
0 g 100
~
if %actor.is_pc%
  say Welcome to the Adventurer's Academia, %actor.name%.
  say Start with the room itself. LOOK shows where you are, and EXITS gives you a quick list of ways out.
  say SCORE shows your condition and progress. INVENTORY and EQUIPMENT show what you carry and what you are wearing.
  say If a command is unfamiliar, HELP followed by that command is usually the best place to begin.
  emote gestures toward the eastern hall.
  say The lessons are arranged to be learned by doing. Take your time and pay attention.
end
~
#18681
Academia - Halric Doors and Keys~
0 g 100
~
if %actor.is_pc%
  emote taps the ring of practice keys at his belt.
  say A closed door is a problem, not a wall. OPEN and CLOSE are the simple lessons.
  say A locked door needs more thought. Carry the right key, then UNLOCK it before you try to OPEN it.
  say The grate here is deliberately locked. If you need a practice key, simply SAY KEY and I will issue one.
  say Use the grate itself to practice UNLOCK, OPEN, CLOSE, and LOCK. I will tell you when you have the lesson right.
end
~
#18682
Academia - Seraphine Recovery~
0 g 100
~
if %actor.is_pc%
  say Preparation is cheaper than panic, %actor.name%.
  say Check INVENTORY before danger. Potions, food, drink, and light sources only help if you remember you have them.
  say Watch your health, mana, and movement. REST when you need to recover, and STAND when you are ready to continue.
  say SLEEP restores you more deeply, but doing it in an unsafe place is an excellent way to become a lesson for someone else.
  emote returns a stoppered vial to its proper rack.
end
~
#18683
Academia - Vael Equipment and Skills~
0 g 100
~
if %actor.is_pc%
  say Before a fight, know what you are carrying and what you actually have equipped.
  say Use INVENTORY for your pack and EQUIPMENT for what is worn. WEAR, WIELD, HOLD, and REMOVE change what you are using.
  say Your SKILLS and SPELLS are tools too. PRACTICE improves what your profession can learn, while TRAIN develops your broader abilities.
  say Different adventurers solve the same danger differently. Learn your own tools instead of copying someone else's habits.
end
~
#18684
Academia - Miriel Communication~
0 g 100
~
if %actor.is_pc%
  say An adventurer who refuses to ask questions usually pays tuition in blood.
  say SAY speaks to the room. TELL speaks directly to another person. Your available CHANNELS can carry conversation farther.
  say HELP is not an admission of ignorance. It is a library you can carry into the field.
  say Read signs, examine unusual details, and speak to people. Useful information is often a better reward than another corpse.
end
~
#18685
Academia - Corren Navigation~
0 g 100
~
if %actor.is_pc%
  emote glances toward the nearest exits before looking back to %actor.name%.
  say Directions are easy. Knowing where those directions place you is the skill.
  say Use LOOK and EXITS, but also remember landmarks, turns, drafts, stairs, and distinctive rooms.
  say The lower examination grounds deliberately stop giving you obvious answers. Build a map in your head instead of walking blindly.
  say If you become uncertain, return to a landmark you recognize before choosing another path.
end
~
#18686
Academia - Aldren Graduation~
0 g 100
~
if %actor.is_pc%
  say The Academia can teach habits, %actor.name%. Judgment only comes from using them.
  say Beyond these halls, Midgaard's Adventurer's Guild offers contracts, training, and a place among others who chose the same road.
  say Beneath the city lies the Old Delve, one of the Guild's first proving grounds for inexperienced adventurers.
  say Do not measure success only by what you kill. Discover something, bring someone home, know when to retreat, and return better prepared.
  say When you are ready, the examination grounds below will test whether the lessons stayed with you.
end
~
#18687
Academia - Practice Beast Combat~
0 k 20
~
emote shifts its weight and feints with one padded horn.
if %actor.is_pc%
  msend %actor% The practice beast's controlled movement reminds you to watch your health and be ready to FLEE if the fight turns against you.
end
~
#18688
Academia - Young Drake Ambient~
0 b 15
~
switch %random.4%
case 1
  emote stretches its small wings and settles them neatly against its back.
  break
case 2
  emote sniffs curiously at a nearby training rack.
  break
case 3
  emote releases a tiny puff of warm air and watches it curl away.
  break
default
  emote tilts its head, bright eyes following movement through the hall.
  break
done
~
#18689
Academia - Spectral Examiner~
0 g 100
~
if %actor.is_pc%
  emote turns its translucent gaze upon %actor.name%.
  say You have reached the final chamber. I will not tell you which lesson mattered most.
  say A living adventurer observes, prepares, chooses, adapts, and knows when pride is more dangerous than retreat.
  say Return to the Graduate's Gate when you have proven that you can leave this course as deliberately as you entered it.
end
~
#18690
Academia - Elowen Fundamentals~
0 c 100
*~
if %actor.is_pc%
  if %cmd% == look
    if !%actor.varexists(academia_seen_look)%
      set academia_seen_look 1
      remote academia_seen_look %actor.id%
      msend %actor% Elowen nods approvingly. Observation comes before action.
    end
  elseif %cmd% == exits
    if !%actor.varexists(academia_seen_exits)%
      set academia_seen_exits 1
      remote academia_seen_exits %actor.id%
      msend %actor% Elowen says, 'Good. Always know how you can leave a room before danger begins.'
    end
  elseif %cmd% == score
    if !%actor.varexists(academia_seen_score)%
      set academia_seen_score 1
      remote academia_seen_score %actor.id%
      msend %actor% Elowen says, 'That is your condition at a glance. Learn to read it before it becomes urgent.'
    end
  elseif %cmd% == inventory
    if !%actor.varexists(academia_seen_inventory)%
      set academia_seen_inventory 1
      remote academia_seen_inventory %actor.id%
      msend %actor% Elowen says, 'Exactly. Know what is in your pack before you need it.'
    end
  elseif %cmd% == equipment
    if !%actor.varexists(academia_seen_equipment)%
      set academia_seen_equipment 1
      remote academia_seen_equipment %actor.id%
      msend %actor% Elowen says, 'Carrying equipment and actually wearing it are two different things.'
    end
  elseif %cmd% == help
    if !%actor.varexists(academia_seen_help)%
      set academia_seen_help 1
      remote academia_seen_help %actor.id%
      msend %actor% Elowen smiles. 'That command will save you more often than pride will.'
    end
  end
  if %actor.varexists(academia_seen_look)% && %actor.varexists(academia_seen_exits)% && %actor.varexists(academia_seen_score)% && %actor.varexists(academia_seen_inventory)% && %actor.varexists(academia_seen_equipment)% && %actor.varexists(academia_seen_help)%
    if !%actor.varexists(academia_fundamentals)%
      set academia_fundamentals 1
      remote academia_fundamentals %actor.id%
      msend %actor% Elowen says, 'Fundamentals complete. You are ready for the practical halls.'
    end
  end
end
return 0
~
#18691
Academia - Halric Practice Key~
0 d 1
key~
if %actor.is_pc%
  mload obj 18608 %actor%
  emote unhooks a brass practice key and hands it to %actor.name%.
  say The key has no value outside this lesson, so do not be afraid to use it.
  say Try UNLOCK GRATE, OPEN GRATE, then descend when you are ready.
  set academia_key_issued 1
  remote academia_key_issued %actor.id%
end
~
#18692
Academia - Halric Door Coaching~
0 r 100
~
if %actor.is_pc% && %direction% == down
  if %cmd% == unlock
    msend %actor% Halric says, 'Good. A lock is only an obstacle when you ignore the mechanism.'
    set academia_door_unlock 1
    remote academia_door_unlock %actor.id%
  elseif %cmd% == open
    msend %actor% Halric says, 'Open only after you understand what is on the other side.'
    set academia_door_open 1
    remote academia_door_open %actor.id%
  elseif %cmd% == close
    msend %actor% Halric says, 'Good habit. Doors change the battlefield and the route behind you.'
    set academia_door_close 1
    remote academia_door_close %actor.id%
  elseif %cmd% == lock
    msend %actor% Halric says, 'And now you understand the whole mechanism.'
    set academia_door_lock 1
    remote academia_door_lock %actor.id%
  end
  if %actor.varexists(academia_door_unlock)% && %actor.varexists(academia_door_open)%
    if !%actor.varexists(academia_doors)%
      set academia_doors 1
      remote academia_doors %actor.id%
      msend %actor% Halric says, 'Doors and keys complete. The Practice Pit below is your next lesson.'
    end
  end
end
~
#18693
Academia - Vael Gear and Skills~
0 c 100
*~
if %actor.is_pc%
  if %cmd% == inventory || %cmd% == equipment
    set academia_gear_checked 1
    remote academia_gear_checked %actor.id%
  elseif %cmd% == wear || %cmd% == wield || %cmd% == hold || %cmd% == remove
    set academia_gear_handled 1
    remote academia_gear_handled %actor.id%
    if !%actor.varexists(academia_gear_praise)%
      set academia_gear_praise 1
      remote academia_gear_praise %actor.id%
      msend %actor% Vael says, 'Good. Equipment is useful only when you know how to put it to work.'
    end
  elseif %cmd% == skills || %cmd% == spells || %cmd% == practice || %cmd% == train
    set academia_skills_checked 1
    remote academia_skills_checked %actor.id%
    if !%actor.varexists(academia_skill_praise)%
      set academia_skill_praise 1
      remote academia_skill_praise %actor.id%
      msend %actor% Vael says, 'Know your profession. The right tool is often a skill rather than a sword.'
    end
  end
  if %actor.varexists(academia_gear_checked)% && %actor.varexists(academia_gear_handled)% && %actor.varexists(academia_skills_checked)%
    if !%actor.varexists(academia_equipment)%
      set academia_equipment 1
      remote academia_equipment %actor.id%
      msend %actor% Vael says, 'Equipment and abilities complete. Adapt what you carry to what you expect to face.'
    end
  end
end
return 0
~
#18694
Academia - Seraphine Recovery Practice~
0 c 100
*~
if %actor.is_pc%
  if %cmd% == rest
    set academia_rested 1
    remote academia_rested %actor.id%
    msend %actor% Seraphine says, 'Recovery is part of adventuring, not time wasted between adventures.'
  elseif %cmd% == sleep
    set academia_slept 1
    remote academia_slept %actor.id%
  elseif %cmd% == wake || %cmd% == stand
    set academia_stood 1
    remote academia_stood %actor.id%
  end
  if %actor.varexists(academia_rested)% && %actor.varexists(academia_stood)%
    if !%actor.varexists(academia_recovery)%
      set academia_recovery 1
      remote academia_recovery %actor.id%
      msend %actor% Seraphine says, 'Recovery complete. Check yourself before every new danger, not after the first mistake.'
    end
  end
end
return 0
~
#18695
Academia - Miriel Communication Practice~
0 c 100
*~
if %actor.is_pc%
  if %cmd% == say
    set academia_used_say 1
    remote academia_used_say %actor.id%
  elseif %cmd% == tell
    set academia_used_tell 1
    remote academia_used_tell %actor.id%
  elseif %cmd% == help
    set academia_used_help_here 1
    remote academia_used_help_here %actor.id%
  elseif %cmd% == channels
    set academia_used_channels 1
    remote academia_used_channels %actor.id%
  end
  if %actor.varexists(academia_used_say)% && %actor.varexists(academia_used_help_here)%
    if !%actor.varexists(academia_communication)%
      set academia_communication 1
      remote academia_communication %actor.id%
      msend %actor% Miriel says, 'Communication complete. Ask questions before uncertainty becomes danger.'
    end
  end
end
return 0
~
#18696
Academia - Corren Navigation Start~
0 q 100
~
if %actor.is_pc%
  if !%actor.varexists(academia_navigation)%
    set academia_navigation 1
    remote academia_navigation %actor.id%
    msend %actor% Corren calls after you, 'Navigation lesson begun. Remember the landmark you just left, not merely the direction you chose.'
  end
end
~
#18697
Academia - Practice Beast Flee Lesson~
0 l 50
~
if %actor.is_pc%
  if !%actor.varexists(academia_combat)%
    set academia_combat 1
    remote academia_combat %actor.id%
    msend %actor% The practice beast backs away for half a heartbeat, giving you room to judge the fight.
    msend %actor% Somewhere above, an instructor's lesson comes back to you: surviving is more important than proving you could have stayed.
  end
end
~
#18698
Academia - Examiner Readiness Check~
0 c 100
*~
if %actor.is_pc% && %cmd% == score
  if %actor.varexists(academia_fundamentals)% && %actor.varexists(academia_doors)% && %actor.varexists(academia_equipment)% && %actor.varexists(academia_recovery)% && %actor.varexists(academia_communication)% && %actor.varexists(academia_navigation)% && %actor.varexists(academia_combat)%
    if !%actor.varexists(academia_examined)%
      set academia_examined 1
      remote academia_examined %actor.id%
      msend %actor% The spectral examiner inclines its head. 'You remembered to judge your own condition before claiming victory. Your examination is complete.'
      msend %actor% The examiner says, 'Return to Provost Aldren and SAY GRADUATE.'
    end
  else
    msend %actor% The spectral examiner says, 'You reached the chamber, but some lessons remain unfinished. Revisit the instructors above and practice what they asked of you.'
  end
end
return 0
~
#18699
Academia - Aldren Graduation Reward~
0 d 1
graduate~
if %actor.is_pc%
  if %actor.varexists(academia_graduate)%
    say Once an Academia graduate, always an Academia graduate, %actor.name%. The Guild and the wider world are waiting.
  elseif %actor.varexists(academia_examined)%
    set academia_graduate 1
    remote academia_graduate %actor.id%
    mload obj 18605 %actor%
    nop %actor.exp(100)%
    nop %actor.gold(3)%
    emote removes a silver-and-blue signet from a small velvet-lined case.
    say By the authority of the Adventurer's Academia, I recognize %actor.name% as a graduate.
    say Take this signet. It is not proof that you know everything. It is proof that you learned how to keep learning.
    msend %actor% You receive an Academia signet ring, 100 experience, and 3 gold.
    say Your next road should lead to Midgaard's Adventurer's Guild. When you want a first real challenge, ask about the Old Delve.
  else
    say Not yet, %actor.name%. Complete the practical lessons and let the spectral examiner judge your readiness first.
  end
end
~
$~
