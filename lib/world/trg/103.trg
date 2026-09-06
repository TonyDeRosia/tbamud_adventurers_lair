#10302
Toybox Pierrot - Sorrow Die~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% @WThe Pierrot lets a black die fall from pale fingers. It settles on @Y%die%@W.@n
  switch %die%
    case 1
      emote bows as though accepting some private tragedy.
    break
    case 2
      eval dmg %self.level% + %random.6%
      %send% %actor% @cA wave of cold melancholy settles over you.@n
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.10%
      %echoaround% %actor% The Pierrot traces a tear beneath its mask and points at %actor.name%.
      %damage% %actor% %dmg%
    break
    case 4
      emote catches the die beneath one heel and slowly shakes its head.
    break
    case 5
      eval dmg %self.level% + %random.12%
      %send% %actor% @WThe Pierrot's painted sorrow becomes suddenly, painfully real.@n
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.20%
      %echo% @CThe black die flashes ivory and the Pierrot's mask splits into a terrible smile.@n
      %damage% %actor% %dmg%
    break
  done
else
  switch %random.3%
    case 1
      emote studies a black die as if waiting for it to apologize.
    break
    case 2
      emote silently rearranges six ivory pips painted on the floor.
    break
    case 3
      emote removes its mask, finds another identical mask beneath it, and sighs.
    break
  done
end
~
#10303
Toybox Harlequin - Painted Die~
0 bk 20
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% @MThe Harlequin flicks a painted die through the air: @Y%die%@M!@n
  switch %die%
    case 1
      emote tumbles backward with an exaggerated gasp.
    break
    case 2
      eval dmg %self.level% + %random.8%
      %damage% %actor% %dmg%
      %echoaround% %actor% The Harlequin rebounds from a wall and clips %actor.name% in passing.
    break
    case 3
      eval dmg %self.level% + %random.12%
      %damage% %actor% %dmg%
      emote bows in the middle of the exchange.
    break
    case 4
      eval dmg %self.level% + %random.16%
      %send% %actor% A blur of crimson and gold catches you from the side!
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.18%
      %damage% %actor% %dmg%
      emote snatches the die out of the air without looking.
    break
    case 6
      eval dmg %self.level% + %random.24%
      say Six! Oh, this is my favorite.
      %damage% %actor% %dmg%
    break
  done
else
  switch %random.4%
    case 1
      emote rolls a die across the back of one gloved hand.
    break
    case 2
      say High roll chooses the next joke.
    break
    case 3
      emote loses a wager to itself and hands its other hand a bell.
    break
    case 4
      say Chance is merely choreography with better suspense.
    break
  done
end
~
#10304
Toybox Grinning Mime - Silent Chance~
0 bk 20
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  emote closes one empty fist and shakes it beside one ear.
  %echo% Something inside the Grinning Mime's empty hand audibly rattles.
  %echo% The mime opens its palm, stares at nothing, and silently shows @Y%die%@n fingers.
  switch %die%
    case 1
      emote recoils from an invisible blow that has not yet been struck.
    break
    case 2
      eval dmg %self.level% + %random.8%
      %send% %actor% An invisible wall slams into you.
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.10%
      emote pulls an invisible rope hand over hand.
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.14%
      %send% %actor% Unseen hands yank your balance sideways.
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.18%
      emote traps something between two invisible panes and squeezes.
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.24%
      emote points at the empty palm, then at you, and the grin widens.
      %damage% %actor% %dmg%
    break
  done
else
  switch %random.4%
    case 1
      emote leans against an invisible wall and watches everyone in silence.
    break
    case 2
      emote mimes rolling dice, then reacts with silent delight to an unseen result.
    break
    case 3
      emote holds one finger to painted lips and smiles.
    break
    case 4
      emote applauds without allowing its gloved hands to make a sound.
    break
  done
end
~
#10305
Toybox Motley Fool - Petty Wagers~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  say Odds, evens, bruises, and bells! I rolled %die%!
  if %die% >= 4
    eval dmg %self.level% + %random.15%
    %damage% %actor% %dmg%
  else
    emote jingles away from danger with theatrical disappointment.
  end
else
  switch %random.4%
    case 1
      say Two bells says the next soldier rolls a one.
    break
    case 2
      emote counts buttons into six tiny piles.
    break
    case 3
      say Never bet your hat. Bet someone else's hat.
    break
    case 4
      emote rolls against itself, loses, and looks genuinely offended.
    break
  done
end
~
#10306
Toybox Court Duelists - Flourish~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% A tiny ivory die skips across the painted floor and stops on @Y%die%@n.
  if %die% == 6
    eval dmg %self.level% + %random.20%
    %send% %actor% The courtier turns the perfect roll into a dazzling finishing flourish!
    %damage% %actor% %dmg%
  elseif %die% >= 3
    eval dmg %self.level% + %random.10%
    %damage% %actor% %dmg%
  else
    emote bows deeply, yielding the moment to chance.
  end
else
  switch %random.3%
    case 1
      emote rehearses a formal bow toward an empty balcony.
    break
    case 2
      emote balances a die on the back of one hand.
    break
    case 3
      emote traces a courtly step around an imaginary opponent.
    break
  done
end
~
#10307
Toybox Marionettes - Strings Decide~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The marionette's strings jerk into a pattern shaped like the number @Y%die%@n.
  if %die% <= 2
    emote collapses bonelessly, then snaps upright again.
  elseif %die% <= 4
    eval dmg %self.level% + %random.8%
    %damage% %actor% %dmg%
  else
    eval dmg %self.level% + %random.16%
    %send% %actor% The puppet lunges exactly where its strings dictate.
    %damage% %actor% %dmg%
  end
else
  switch %random.3%
    case 1
      emote hangs motionless until unseen strings suddenly correct its posture.
    break
    case 2
      emote turns its head a fraction too far toward a distant rattle of dice.
    break
    case 3
      emote bows toward an unseen puppeteer.
    break
  done
end
~
#10308
Toybox Puppetmaster - Bone Dice~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The Puppetmaster rolls a bone die along the control bar: @Y%die%@n.
  switch %die%
    case 1
      emote curses softly as several strings tangle together.
    break
    case 2
      eval dmg %self.level% + %random.10%
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.14%
      %send% %actor% Black strings snap around your limbs for a painful instant.
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.18%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.22%
      %echoaround% %actor% The Puppetmaster yanks both hands apart and %actor.name% stumbles violently.
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.28%
      %echo% Every nearby string snaps taut at once.
      %damage% %actor% %dmg%
    break
  done
else
  emote rolls bone dice to decide which hanging puppet receives the next tug.
end
~
#10309
Toybox Stringcrawler - Tangled Pips~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  if %die% >= 4
    eval dmg %self.level% + %random.12%
    %send% %actor% A loop of living string lashes tight around you.
    %damage% %actor% %dmg%
  else
    emote knots itself into a six-pointed snarl before skittering sideways.
  end
else
  emote knots and unknots itself around a tiny wooden die.
end
~
#10310
Toybox Soldiers - Command Dice~
0 bk 20
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The toy soldier checks a painted command die showing @Y%die%@n.
  switch %die%
    case 1
      emote snaps into a rigid guard stance.
    break
    case 2
      eval dmg %self.level% + %random.6%
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.9%
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.12%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.16%
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.20%
      %echo% @RThe soldier stamps once: SIXTH FORMATION!@n
      %damage% %actor% %dmg%
    break
  done
else
  switch %random.3%
    case 1
      emote rolls a command die and immediately changes its guard posture.
    break
    case 2
      emote salutes another toy soldier across the room.
    break
    case 3
      emote checks its painted boots for proper alignment.
    break
  done
end
~
#10314
Toybox Officers - Formation Roll~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The officer casts a command die across a shield: @Y%die%@n.
  switch %die%
    case 1
      say First formation! Hold!
    break
    case 2
      say Second formation! Press!
      eval dmg %self.level% + %random.10%
      %damage% %actor% %dmg%
    break
    case 3
      say Third formation! Shields!
    break
    case 4
      say Fourth formation! Wheel!
      eval dmg %self.level% + %random.14%
      %damage% %actor% %dmg%
    break
    case 5
      say Fifth formation! Charge!
      eval dmg %self.level% + %random.18%
      %damage% %actor% %dmg%
    break
    case 6
      say Sixth formation! No mercy!
      eval dmg %self.level% + %random.24%
      %damage% %actor% %dmg%
    break
  done
else
  emote rolls a command die, studies it, and points a patrol in a new direction.
end
~
#10315
Toybox Dolls - Tea and Chance~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The doll lets a tiny porcelain die click across the floor: @Y%die%@n.
  if %die% == 1
    emote covers its mouth in delicate surprise as a new crack appears.
  elseif %die% <= 4
    eval dmg %self.level% + %random.12%
    %damage% %actor% %dmg%
  else
    eval dmg %self.level% + %random.20%
    %send% %actor% Porcelain fingers strike with sudden, brittle force.
    %damage% %actor% %dmg%
  end
else
  switch %random.3%
    case 1
      emote adjusts a teacup by less than the width of a fingernail.
    break
    case 2
      emote rolls a tiny die to determine where it should sit.
    break
    case 3
      emote turns its glass eyes toward you without moving its head.
    break
  done
end
~
#10316
Toybox Stuffed Beasts - Button Luck~
0 bk 18
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  if %die% >= 4
    eval dmg %self.level% + %random.16%
    %send% %actor% The stuffed beast crashes into you in a storm of yarn and buttons.
    %damage% %actor% %dmg%
  else
    emote shakes loose a button, paws it like a die, and seems dissatisfied with the result.
  end
else
  emote nudges a loose button across the floor and watches where it stops.
end
~
#10318
Toybox Surprise Toys - Spring Loaded~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% Somewhere inside the toy, something rattles and settles on @Y%die%@n.
  if %die% <= 2
    %echo% @Wclick... click... click...@n
  elseif %die% <= 5
    eval dmg %self.level% + %random.14%
    %damage% %actor% %dmg%
  else
    eval dmg %self.level% + %random.26%
    %echo% @RBOING!@n The enchanted toy erupts forward with impossible force.
    %damage% %actor% %dmg%
  end
else
  %echo% @Wclick...@n
end
~
#10319
Toybox Living Dice and Cards~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  %echo% The enchanted game-piece tumbles and presents @Y%die%@n.
  if %die% == 1
    emote wobbles in apparent embarrassment.
  elseif %die% == 6
    eval dmg %self.level% + %random.24%
    %damage% %actor% %dmg%
  else
    eval dmg %self.level% + %random.12%
    %damage% %actor% %dmg%
  end
else
  emote shifts position as though some unseen player has taken a turn.
end
~
#10320
Toybox Chess Pieces - Board Law~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval move %random.6%
  %echo% The chess piece pauses as if considering move @Y%move%@n.
  if %move% <= 2
    emote repositions with cold geometric precision.
  elseif %move% <= 5
    eval dmg %self.level% + %random.14%
    %damage% %actor% %dmg%
  else
    eval dmg %self.level% + %random.24%
    %echo% @WThe piece finds a perfect line of attack.@n
    %damage% %actor% %dmg%
  end
else
  emote shifts exactly one legal move and becomes motionless again.
end
~
#10321
Toybox House Dealer - NPC Gambling~
0 b 20
~
switch %random.6%
  case 1
    emote rattles two ivory dice beneath a silver cup.
  break
  case 2
    say The House observes. The House does not invite.
  break
  case 3
    emote watches a Harlequin surrender three bells after a poor roll.
  break
  case 4
    emote records a wager between two toy soldiers in a narrow ledger.
  break
  case 5
    say Doubles again. The Doll Governess will be displeased.
  break
  case 6
    emote taps the felt twice and a pair of living dice roll themselves.
  break
done
~
#10322
Toybox Royal Fool - Decree of Dice~
0 bk 22
~
if %self.fighting%
  set actor %self.fighting%
  eval die %random.6%
  say The court decrees %die%!
  if %die% >= 4
    eval dmg %self.level% + %random.20%
    %damage% %actor% %dmg%
  else
    emote performs an elaborate bow to the authority of the die.
  end
else
  emote rolls a gilded die to decide which bell on its cap to ring.
end
~
#10323
Boss - Grand Harlequin Six Colors~
0 k 35
~
set actor %self.fighting%
if %actor%
  eval die %random.6%
  %echo% @MThe Grand Harlequin sends six colored dice spinning overhead. One drops: @Y%die%@M.@n
  switch %die%
    case 1
      emote slips on purpose, rolling away with insulting grace.
    break
    case 2
      eval dmg %self.level% + %random.20%
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.25%
      %send% %actor% A ribbon-blurred feint becomes a real strike.
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.30%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.35%
      say Five colors, five lies, one bruise!
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.45%
      say Six! Curtain call!
      %damage% %actor% %dmg%
    break
  done
end
~
#10329
Boss - Grand Puppeteer Bone Roll~
0 k 35
~
set actor %self.fighting%
if %actor%
  eval die %random.6%
  %echo% The Grand Puppeteer rolls a bone die along a black control bar: @Y%die%@n.
  switch %die%
    case 1
      emote jerks one string and an unseen puppet shrieks somewhere backstage.
    break
    case 2
      eval dmg %self.level% + %random.20%
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.28%
      %send% %actor% Strings lash around your wrists and yank hard.
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.34%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.40%
      %echoaround% %actor% Black cords snap taut around %actor.name%.
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.50%
      %echo% Every string in the workshop pulls at once.
      %damage% %actor% %dmg%
    break
  done
end
~
#10330
Boss - General Jack Command Die~
0 k 35
~
set actor %self.fighting%
if %actor%
  eval die %random.6%
  %echo% @YGeneral Jack-A-Napes rolls his command die across the war table: %die%.@n
  switch %die%
    case 1
      say FIRST FORMATION! HOLD THE LINE!
    break
    case 2
      say SECOND FORMATION! ADVANCE!
      eval dmg %self.level% + %random.22%
      %damage% %actor% %dmg%
    break
    case 3
      say THIRD FORMATION! SHIELDS!
    break
    case 4
      say FOURTH FORMATION! WHEEL!
      eval dmg %self.level% + %random.30%
      %damage% %actor% %dmg%
    break
    case 5
      say FIFTH FORMATION! CHARGE!
      eval dmg %self.level% + %random.40%
      %damage% %actor% %dmg%
    break
    case 6
      say SIXTH FORMATION! BREAK THEM!
      eval dmg %self.level% + %random.52%
      %damage% %actor% %dmg%
    break
  done
end
~
#10331
Boss - Madame Porcelaine Tea Die~
0 k 35
~
set actor %self.fighting%
if %actor%
  eval die %random.6%
  %echo% Madame Porcelaine drops a tiny white die into an empty teacup. It shows @Y%die%@n.
  switch %die%
    case 1
      emote notices a fresh crack in her wrist and looks deeply offended.
    break
    case 2
      eval heal %self.level% + %random.20%
      %damage% %self% -%heal%
      say Tea restores composure.
    break
    case 3
      eval dmg %self.level% + %random.25%
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.32%
      %send% %actor% A porcelain saucer shatters against you like a thrown blade.
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.40%
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.52%
      say Six. How terribly impolite for you.
      %damage% %actor% %dmg%
    break
  done
end
~
#10332
Boss - Black Queen Calculates~
0 k 35
~
set actor %self.fighting%
if %actor%
  eval move %random.6%
  %echo% The Black Chess Queen considers line @Y%move%@n without moving her expression.
  switch %move%
    case 1
      emote glides one square away, forcing the board to realign around her.
    break
    case 2
      eval dmg %self.level% + %random.25%
      %damage% %actor% %dmg%
    break
    case 3
      eval dmg %self.level% + %random.32%
      %send% %actor% The Queen attacks along a perfect diagonal.
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.38%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.45%
      %echo% CHECK.
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.60%
      %echo% @RThe Black Queen finds checkmate in a single brutal line.@n
      %damage% %actor% %dmg%
    break
  done
end
~
#10339
Boss - Croupier Two Honest Dice~
0 k 40
~
set actor %self.fighting%
if %actor%
  eval die1 %random.6%
  eval die2 %random.6%
  eval total %die1% + %die2%
  %echo% @WThe Croupier rattles two ivory dice beneath a silver cup and casts them across the felt.@n
  %echo% @Y[ %die1% ] [ %die2% ]  Total: %total%@n
  switch %total%
    case 2
      say Snake eyes. The House pays for its arrogance.
      eval backfire %self.level% + %random.20%
      %damage% %self% %backfire%
    break
    case 3
      eval dmg %self.level% + %random.12%
      %damage% %actor% %dmg%
    break
    case 4
      eval dmg %self.level% + %random.16%
      %damage% %actor% %dmg%
    break
    case 5
      eval dmg %self.level% + %random.20%
      %damage% %actor% %dmg%
    break
    case 6
      eval dmg %self.level% + %random.24%
      %echo% The Croupier taps the six-pip face twice.
      %damage% %actor% %dmg%
    break
    case 7
      say Seven. The House wins.
      eval dmg %self.level% + %random.45%
      %damage% %actor% %dmg%
    break
    case 8
      eval dmg %self.level% + %random.28%
      %damage% %actor% %dmg%
    break
    case 9
      eval dmg %self.level% + %random.32%
      %damage% %actor% %dmg%
    break
    case 10
      eval dmg %self.level% + %random.38%
      %damage% %actor% %dmg%
    break
    case 11
      eval heal %self.level% + %random.30%
      %damage% %self% -%heal%
      say Eleven. The House recovers.
    break
    case 12
      say Boxcars.
      eval dmg %self.level% + %random.70%
      %echo% @RBoth dice blaze as the Croupier sweeps the table clean.@n
      %damage% %actor% %dmg%
    break
  done
end
~
#10345
Boss - Jester Fool's Die~
0 k 38
~
set actor %self.fighting%
if %actor%
  if %cheated%
    eval die1 %random.6%
    eval die2 %random.6%
    eval total %die1% + %die2%
    %echo% @RThe Jester throws TWO ivory dice: @Y%die1% @Rand @Y%die2%@R. He grins at the total of @Y%total%@R.@n
    if %total% >= 9
      eval dmg %self.level% + %random.80%
      %damage% %actor% %dmg%
    elseif %total% <= 4
      eval heal %self.level% + %random.35%
      %damage% %self% -%heal%
    else
      eval dmg %self.level% + %random.45%
      %damage% %actor% %dmg%
    end
  else
    eval die %random.6%
    %echo% @WThe Jester kicks the enormous Fool's Die into the air. It crashes down showing @Y%die%@W.@n
    switch %die%
      case 1
        eval dmg %self.level% + %random.20%
        say Pierrot's sorrow.
        %damage% %actor% %dmg%
      break
      case 2
        eval dmg %self.level% + %random.28%
        say Puppet strings.
        %damage% %actor% %dmg%
      break
      case 3
        eval dmg %self.level% + %random.34%
        say Painted legion.
        %damage% %actor% %dmg%
      break
      case 4
        eval heal %self.level% + %random.30%
        say Porcelain tea.
        %damage% %self% -%heal%
      break
      case 5
        eval dmg %self.level% + %random.45%
        say The House of Chance.
        %damage% %actor% %dmg%
      break
      case 6
        eval dmg %self.level% + %random.70%
        say The Fool's privilege.
        %damage% %actor% %dmg%
      break
    done
  end
end
~
#10355
Boss - Jester Cheats Below Thirty~
0 l 30
~
if !%cheated%
  set cheated 1
  global cheated
  %echo% @RThe Jester catches the Fool's Die before it finishes rolling.@n
  say Oh, we've played by the rules long enough.
  emote turns the die deliberately to six and slips a second die from one sleeve.
  %echo% Every bell in the Toybox rings once.
end
~
#10362
Toybox Ambient - Open Chest~
2 b 14
~
switch %random.6%
  case 1
    %echo% Somewhere beneath the blocks, a die rattles once and goes still.
  break
  case 2
    %echo% A tiny bell answers another bell far across the Toybox.
  break
  case 3
    %echo% One painted block quietly turns itself to show a different letter.
  break
  case 4
    %echo% A ribbon lifts as though something small just passed beneath it.
  break
  case 5
    %echo% The wooden floor creaks under a weight much larger than any visible toy.
  break
  case 6
    %echo% Six pale pips glow briefly on a nearby block, then fade.
  break
done
~
#10363
Toybox Ambient - Motley Court~
2 b 16
~
switch %random.6%
  case 1
    %echo% A wave of bells moves through the court without any visible cause.
  break
  case 2
    %echo% A Harlequin somewhere nearby shouts, 'High roll keeps the mask!'
  break
  case 3
    %echo% Quiet applause begins behind a wall and ends after exactly six claps.
  break
  case 4
    %echo% A black die rolls through the room, turns a corner, and disappears.
  break
  case 5
    %echo% Two unseen courtiers argue fiercely over whether a tilted die counts.
  break
  case 6
    %echo% One of the hanging masks slowly changes from laughter to tears.
  break
done
~
#10364
Toybox Ambient - Marionette Theatre~
2 b 16
~
switch %random.6%
  case 1
    %echo% Several puppet strings tighten at once, though their marionettes do not move.
  break
  case 2
    %echo% A wooden audience applauds from somewhere beyond the curtain.
  break
  case 3
    %echo% A bone die clatters across the rafters overhead.
  break
  case 4
    %echo% One hanging puppet turns its head toward you and immediately goes limp.
  break
  case 5
    %echo% The crimson curtain billows as if someone enormous passed behind it.
  break
  case 6
    %echo% A control bar swings slowly even though every string beneath it is still.
  break
done
~
#10376
Toybox Ambient - Painted Citadel~
2 b 16
~
switch %random.6%
  case 1
    %echo% Wooden boots strike the floor in perfect marching cadence.
  break
  case 2
    %echo% A captain calls a formation number and dozens of toy voices answer.
  break
  case 3
    %echo% A command die bounces off a shield somewhere nearby.
  break
  case 4
    %echo% Tiny signal flags change position along the battlements.
  break
  case 5
    %echo% A nutcracker jaw closes with a sharp wooden CRACK.
  break
  case 6
    %echo% A distant drumroll ends the instant an unseen die stops rolling.
  break
done
~
#10389
Toybox Ambient - Dollhouse~
2 b 16
~
switch %random.6%
  case 1
    %echo% A porcelain cup quietly turns itself toward you.
  break
  case 2
    %echo% Tiny footsteps cross the ceiling and stop directly overhead.
  break
  case 3
    %echo% A child's music box plays three notes, then winds itself backward.
  break
  case 4
    %echo% One doll is facing a different direction than it was a moment ago.
  break
  case 5
    %echo% Something soft breathes inside a nearby wardrobe.
  break
  case 6
    %echo% A porcelain die clicks once inside an empty teacup.
  break
done
~
#10390
Toybox Ambient - House of Chance~
2 b 18
~
switch %random.6%
  case 1
    %echo% Dice rattle beneath several cups at once, followed by a chorus of groans.
  break
  case 2
    %echo% A dealer announces, 'Seven. The House records another favor owed.'
  break
  case 3
    %echo% Two toy soldiers argue over a cocked die until a Pierrot silently rerolls it.
  break
  case 4
    %echo% A pair of living dice bounce through the room chasing one another.
  break
  case 5
    %echo% A Harlequin loses a bell, removes it from his cap, and slides it across the felt.
  break
  case 6
    %echo% Someone calls 'Boxcars!' and the whole hall erupts in bells.
  break
done
~
#10391
Toybox Ambient - Grand Gameboard~
2 b 18
~
switch %random.6%
  case 1
    %echo% A chess piece moves somewhere out of sight with a heavy wooden scrape.
  break
  case 2
    %echo% A painted herald calls, 'Check!' from across the board.
  break
  case 3
    %echo% Ivory pawns advance one square in perfect unison.
  break
  case 4
    %echo% A black knight lands two squares away with a hollow crack.
  break
  case 5
    %echo% The lines between several squares glow briefly, marking a threatened path.
  break
  case 6
    %echo% Every piece on the board becomes perfectly still, as though awaiting a player's hand.
  break
done
~
#10392
Toybox Ambient - Behind the Smile~
2 b 22
~
switch %random.6%
  case 1
    %echo% One enormous die rolls somewhere ahead, though no one seems to have thrown it.
  break
  case 2
    %echo% Every bell nearby rings once and then refuses to move.
  break
  case 3
    %echo% A crimson smile appears for an instant in a dark mirror.
  break
  case 4
    %echo% The distant Jester hums a tune without any melody you can remember.
  break
  case 5
    %echo% Six pale pips appear beneath your feet and vanish one by one.
  break
  case 6
    %echo% A voice far ahead whispers, 'Your turn.'
  break
done
~
$~
