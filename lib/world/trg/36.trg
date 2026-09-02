#3600
Zone36 Board - Distant Move~
2 b 4
~
switch %random.6%
  case 1
    %echo% A deep stone scrape rolls across the board, followed by a single bell-like note.
  break
  case 2
    %echo% Several buried rune-lines flash from square to square before fading.
  break
  case 3
    %echo% Far away, something enormous strikes marble hard enough to tremble beneath your feet.
  break
  case 4
    %echo% A whisper of old magic passes over the board: @w...move acknowledged...@n
  break
  case 5
    %echo% Chips of black and ivory stone rattle briefly in the seams between squares.
  break
  case 6
    %echo% For a heartbeat, the entire field seems to be waiting for the next move.
  break
done
~
#3601
Zone36 Royal Rank - Old Command~
2 b 4
~
switch %random.5%
  case 1
    %echo% An ancient voice whispers across the royal rank: @w'Protect the King.'@n
  break
  case 2
    %echo% A crown-shaped sigil appears beneath the marble and slowly fades.
  break
  case 3
    %echo% The opposing monarch seems to turn its head by the smallest possible amount.
  break
  case 4
    %echo% Somewhere below, treasury wards answer with a dull metallic chime.
  break
  case 5
    %echo% The royal rank vibrates as though remembering a command issued centuries ago.
  break
done
~
#3602
Zone36 Obsidian Pawn - Discipline~
0 b 5
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote lowers its chipped shield and locks its stone feet into the square.
  break
  case 2
    emote turns its expressionless helm toward the opposing rank.
  break
  case 3
    emote scrapes its sword edge slowly across its shield.
  break
  case 4
    emote whispers, 'Advance. Hold. Fall. Return.'
  break
done
~
#3603
Zone36 Ivory Pawn - Discipline~
0 b 5
~
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    emote raises its pale shield into perfect formation.
  break
  case 2
    emote turns its smooth marble face toward the northern ranks.
  break
  case 3
    emote taps its sword once against its shield; a golden rune answers.
  break
  case 4
    emote intones, 'The line advances when the Game commands.'
  break
done
~
#3604
Zone36 Rook - Siege Response~
0 k 12
~
switch %random.4%
  case 1
    emote drops its fortress-shield and drives straight forward like a moving wall!
    bash %actor%
  break
  case 2
    emote locks its rune-wheels into line; stone screams across the marble.
  break
  case 3
    emote booms, 'FILE SECURED.'
  break
  case 4
    emote rotates its battlemented body toward %actor.name%.
  break
done
~
#3605
Zone36 Knight - Crooked Leap~
0 k 12
~
switch %random.4%
  case 1
    emote vanishes in a burst of runes and crashes down from an impossible angle!
    bash %actor%
  break
  case 2
    emote stamps twice, then lunges where no straight charge should reach.
  break
  case 3
    emote lowers its lance while geometric light gathers beneath its hooves.
  break
  case 4
    emote seems to skip the space between two positions rather than crossing it.
  break
done
~
#3606
Zone36 Bishop - Diagonal Hex~
0 k 10
~
switch %random.4%
  case 1
    emote draws a burning diagonal through the air with its staff.
    dg_cast 'blindness' %actor%
  break
  case 2
    emote aligns a chain of sigils through several distant squares.
  break
  case 3
    emote whispers, 'The diagonal remains unbroken.'
  break
  case 4
    emote folds both stone hands around its staff as runes converge on %actor.name%.
  break
done
~
#3607
Zone36 Queen - Dominion~
0 k 9
~
switch %random.5%
  case 1
    emote glides across the square with impossible speed and strikes from the flank!
    bash %actor%
  break
  case 2
    emote raises one hand as rune-lines race outward in every direction.
    dg_cast 'blindness' %actor%
  break
  case 3
    emote says, 'Every line on this board answers to the Crown.'
  break
  case 4
    emote turns without moving, somehow facing every approach at once.
  break
  case 5
    emote's crystal crown burns with contained magical fire.
  break
done
~
#3608
Zone36 King - Royal Ward~
0 k 10
~
switch %random.5%
  case 1
    emote strikes its sceptre against the marble; layered wards lock into place.
    dg_cast 'evasion'
  break
  case 2
    emote booms, 'THE KING REMAINS.'
  break
  case 3
    emote's orbiting ward-runes tighten around its massive frame.
  break
  case 4
    emote points its sceptre toward %actor.name% and the entire square trembles.
  break
  case 5
    emote turns toward its distant ranks as though issuing a silent command.
  break
done
~
#3609
Zone36 Arbiter - Observation~
0 b 5
~
if %self.fighting%
  halt
end
switch %random.5%
  case 1
    emote turns its crystal core toward the northern King, then the southern King.
  break
  case 2
    emote says, 'The Game continues because neither victory condition has been satisfied.'
  break
  case 3
    emote traces a perfect square in the air; tiny move-notations spin inside it.
  break
  case 4
    emote says, 'Pieces fail. Positions remain.'
  break
  case 5
    emote remains perfectly still while both black and pale runes orbit its body.
  break
done
~
#3610
Zone36 Rune Wisp - Flicker~
0 b 6
~
switch %random.4%
  case 1
    emote breaks into four lines of light and reforms one square-width away.
  break
  case 2
    emote sketches a tiny glowing knight's move in the air.
  break
  case 3
    emote pulses in answer to a distant rune-line.
  break
  case 4
    emote dims until only a geometric afterimage remains.
  break
done
~
#3611
Zone36 Shattered Husk - Broken Orders~
0 b 6
~
switch %random.4%
  case 1
    emote drags one broken leg forward and rasps, 'Advance.'
  break
  case 2
    emote raises an arm where a shield is no longer attached.
  break
  case 3
    emote freezes suddenly as a damaged command-rune flashes across its chest.
  break
  case 4
    emote turns in a slow circle, searching for a formation that no longer exists.
  break
done
~
#3612
Zone36 Scavenger - Core Hunter~
0 b 6
~
switch %random.4%
  case 1
    emote chips greedily at a fragment of enchanted marble.
  break
  case 2
    emote stuffs a glowing rune-chip into a pouch beneath one stone wing.
  break
  case 3
    emote clicks its granite teeth and watches for weakened constructs.
  break
  case 4
    emote sniffs the air for loose enchantment.
  break
done
~
#3613
Zone36 Fallen Champion - Unbound Charge~
0 k 12
~
switch %random.4%
  case 1
    emote charges without regard for rank, file, or allegiance!
    bash %actor%
  break
  case 2
    emote's fractured runes flare in contradictory colors.
  break
  case 3
    emote roars, 'MOVE INVALID. MOVE INVALID. MOVE INVALID.'
  break
  case 4
    emote attacks from an angle no sane piece would choose.
  break
done
~
#3614
Zone36 Center - Rune Convergence~
2 b 5
~
switch %random.4%
  case 1
    %echo% Four buried lines of magic meet beneath the central squares, then snap dark.
  break
  case 2
    %echo% A translucent chess notation hangs in the air for a moment before dissolving.
  break
  case 3
    %echo% Somewhere beneath the board, ancient mechanisms count one more move.
  break
  case 4
    %echo% The center files hum with equal traces of Obsidian and Ivory magic.
  break
done
~
#3615
Zone36 Vault - Crown Memory~
2 b 5
~
switch %random.4%
  case 1
    %echo% The treasury wards brighten as though recognizing an old royal presence.
  break
  case 2
    %echo% A sealed niche clicks once behind the stone and falls silent.
  break
  case 3
    %echo% Ancient victory tablets whisper against one another in the still air.
  break
  case 4
    %echo% A crown-shaped reflection moves across the wall without a source.
  break
done
~
#3616
Zone36 Obsidian Vault - Reset Loot~
2 f 100
~
* ------------------------------------------------------------
* Remove the previous Obsidian vault roll without purging
* unrelated/player-dropped objects.
* ------------------------------------------------------------
set item %self.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum% == 3616 || %item.vnum% == 3618 || %item.vnum% == 3620 || %item.vnum% == 3622 || %item.vnum% == 3624 || %item.vnum% == 3626 || %item.vnum% == 3628 || %item.vnum% == 3629 || %item.vnum% == 3630 || %item.vnum% == 3632 || %item.vnum% == 3633 || %item.vnum% == 3636 || %item.vnum% == 3638
    %purge% %item%
  end
  set item %next_item%
done
* 20 Poor / 40 Modest / 25 Rich / 12 Royal / 3 Legendary
eval tier %random.100%
if %tier% <= 20
  * POOR: one guaranteed low relic, sometimes a second, rare basic gear.
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3630
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3638
    break
  done
  if %random.100% <= 50
    switch %random.4%
      case 1
        %load% obj 3628
      break
      case 2
        %load% obj 3629
      break
      case 3
        %load% obj 3630
      break
      case 4
        %load% obj 3632
      break
    done
  end
  if %random.100% <= 15
    switch %random.3%
      case 1
        %load% obj 3618
      break
      case 2
        %load% obj 3620
      break
      case 3
        %load% obj 3622
      break
    done
  end
elseif %tier% <= 60
  * MODEST: low relic + guaranteed common court gear.
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3630
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3638
    break
  done
  switch %random.3%
    case 1
      %load% obj 3618
    break
    case 2
      %load% obj 3620
    break
    case 3
      %load% obj 3622
    break
  done
  if %random.100% <= 35
    switch %random.3%
      case 1
        %load% obj 3618
      break
      case 2
        %load% obj 3620
      break
      case 3
        %load% obj 3622
      break
    done
  end
  if %random.100% <= 10
    switch %random.4%
      case 1
        %load% obj 3624
      break
      case 2
        %load% obj 3626
      break
      case 3
        %load% obj 3633
      break
      case 4
        %load% obj 3636
      break
    done
  end
elseif %tier% <= 85
  * RICH: common gear plus a guaranteed signature reward.
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3630
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3638
    break
  done
  switch %random.3%
    case 1
      %load% obj 3618
    break
    case 2
      %load% obj 3620
    break
    case 3
      %load% obj 3622
    break
  done
  switch %random.4%
    case 1
      %load% obj 3624
    break
    case 2
      %load% obj 3626
    break
    case 3
      %load% obj 3633
    break
    case 4
      %load% obj 3636
    break
  done
  if %random.100% <= 45
    switch %random.3%
      case 1
        %load% obj 3618
      break
      case 2
        %load% obj 3620
      break
      case 3
        %load% obj 3622
      break
    done
  end
  if %random.100% <= 25
    switch %random.4%
      case 1
        %load% obj 3624
      break
      case 2
        %load% obj 3626
      break
      case 3
        %load% obj 3633
      break
      case 4
        %load% obj 3636
      break
    done
  end
  if %random.100% <= 20
    %load% obj 3616
  end
elseif %tier% <= 97
  * ROYAL: several meaningful rewards and a strong hoard chance.
  %load% obj 3638
  switch %random.3%
    case 1
      %load% obj 3618
    break
    case 2
      %load% obj 3620
    break
    case 3
      %load% obj 3622
    break
  done
  if %random.100% <= 70
    %load% obj 3618
  end
  if %random.100% <= 70
    %load% obj 3620
  end
  if %random.100% <= 70
    %load% obj 3622
  end
  switch %random.4%
    case 1
      %load% obj 3624
    break
    case 2
      %load% obj 3626
    break
    case 3
      %load% obj 3633
    break
    case 4
      %load% obj 3636
    break
  done
  if %random.100% <= 55
    switch %random.4%
      case 1
        %load% obj 3624
      break
      case 2
        %load% obj 3626
      break
      case 3
        %load% obj 3633
      break
      case 4
        %load% obj 3636
      break
    done
  end
  if %random.100% <= 50
    %load% obj 3616
  end
else
  * LEGENDARY: jackpot. The royal hoard is guaranteed.
  %load% obj 3616
  %load% obj 3638
  %load% obj 3618
  %load% obj 3620
  %load% obj 3622
  if %random.100% <= 80
    %load% obj 3624
  end
  if %random.100% <= 80
    %load% obj 3626
  end
  if %random.100% <= 80
    %load% obj 3633
  end
  if %random.100% <= 80
    %load% obj 3636
  end
  if %random.100% <= 60
    %load% obj 3630
  end
  if %random.100% <= 60
    %load% obj 3632
  end
end
~
#3617
Zone36 Ivory Vault - Reset Loot~
2 f 100
~
* ------------------------------------------------------------
* Remove the previous Ivory vault roll without purging
* unrelated/player-dropped objects.
* ------------------------------------------------------------
set item %self.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum% == 3617 || %item.vnum% == 3619 || %item.vnum% == 3621 || %item.vnum% == 3623 || %item.vnum% == 3625 || %item.vnum% == 3627 || %item.vnum% == 3628 || %item.vnum% == 3629 || %item.vnum% == 3631 || %item.vnum% == 3632 || %item.vnum% == 3634 || %item.vnum% == 3637 || %item.vnum% == 3639
    %purge% %item%
  end
  set item %next_item%
done
* 20 Poor / 40 Modest / 25 Rich / 12 Royal / 3 Legendary
eval tier %random.100%
if %tier% <= 20
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3631
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3639
    break
  done
  if %random.100% <= 50
    switch %random.4%
      case 1
        %load% obj 3628
      break
      case 2
        %load% obj 3629
      break
      case 3
        %load% obj 3631
      break
      case 4
        %load% obj 3632
      break
    done
  end
  if %random.100% <= 15
    switch %random.3%
      case 1
        %load% obj 3619
      break
      case 2
        %load% obj 3621
      break
      case 3
        %load% obj 3623
      break
    done
  end
elseif %tier% <= 60
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3631
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3639
    break
  done
  switch %random.3%
    case 1
      %load% obj 3619
    break
    case 2
      %load% obj 3621
    break
    case 3
      %load% obj 3623
    break
  done
  if %random.100% <= 35
    switch %random.3%
      case 1
        %load% obj 3619
      break
      case 2
        %load% obj 3621
      break
      case 3
        %load% obj 3623
      break
    done
  end
  if %random.100% <= 10
    switch %random.4%
      case 1
        %load% obj 3625
      break
      case 2
        %load% obj 3627
      break
      case 3
        %load% obj 3634
      break
      case 4
        %load% obj 3637
      break
    done
  end
elseif %tier% <= 85
  switch %random.5%
    case 1
      %load% obj 3628
    break
    case 2
      %load% obj 3629
    break
    case 3
      %load% obj 3631
    break
    case 4
      %load% obj 3632
    break
    case 5
      %load% obj 3639
    break
  done
  switch %random.3%
    case 1
      %load% obj 3619
    break
    case 2
      %load% obj 3621
    break
    case 3
      %load% obj 3623
    break
  done
  switch %random.4%
    case 1
      %load% obj 3625
    break
    case 2
      %load% obj 3627
    break
    case 3
      %load% obj 3634
    break
    case 4
      %load% obj 3637
    break
  done
  if %random.100% <= 45
    switch %random.3%
      case 1
        %load% obj 3619
      break
      case 2
        %load% obj 3621
      break
      case 3
        %load% obj 3623
      break
    done
  end
  if %random.100% <= 25
    switch %random.4%
      case 1
        %load% obj 3625
      break
      case 2
        %load% obj 3627
      break
      case 3
        %load% obj 3634
      break
      case 4
        %load% obj 3637
      break
    done
  end
  if %random.100% <= 20
    %load% obj 3617
  end
elseif %tier% <= 97
  %load% obj 3639
  switch %random.3%
    case 1
      %load% obj 3619
    break
    case 2
      %load% obj 3621
    break
    case 3
      %load% obj 3623
    break
  done
  if %random.100% <= 70
    %load% obj 3619
  end
  if %random.100% <= 70
    %load% obj 3621
  end
  if %random.100% <= 70
    %load% obj 3623
  end
  switch %random.4%
    case 1
      %load% obj 3625
    break
    case 2
      %load% obj 3627
    break
    case 3
      %load% obj 3634
    break
    case 4
      %load% obj 3637
    break
  done
  if %random.100% <= 55
    switch %random.4%
      case 1
        %load% obj 3625
      break
      case 2
        %load% obj 3627
      break
      case 3
        %load% obj 3634
      break
      case 4
        %load% obj 3637
      break
    done
  end
  if %random.100% <= 50
    %load% obj 3617
  end
else
  %load% obj 3617
  %load% obj 3639
  %load% obj 3619
  %load% obj 3621
  %load% obj 3623
  if %random.100% <= 80
    %load% obj 3625
  end
  if %random.100% <= 80
    %load% obj 3627
  end
  if %random.100% <= 80
    %load% obj 3634
  end
  if %random.100% <= 80
    %load% obj 3637
  end
  if %random.100% <= 60
    %load% obj 3631
  end
  if %random.100% <= 60
    %load% obj 3632
  end
end
~
$~
