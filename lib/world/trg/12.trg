#1200
UTILITY - Trigger Keeper~
2 a 100
~
* Not used for anything but variable storage. No Script!
* Without this trig attached you will get the error:
* Trying to access Global var list of void. Apparently this has not been set up!
~
#1201
TRAINING - Speech Calculator~
2 d 100
*~
* By Mordecai
if %actor.is_pc%
  return 0
  eval sum %speech%
  eval test1 "%speech%"
  eval test %test1.strlen%
  eval che %sum%/1
  if %che% == %sum%
    %echo% 	WComputing results...	n
    if (%speech%/===)
      if (%sum%==1)
        set sum Yes
      elseif (%sum%==0)
        set sum No
      end
    end
    eval st 2+%test%
    eval o .
    eval sumslen "%sum%
    eval len %st% - (%sumslen.strlen%-2)
    if %len% > 0
      eval dif (%len%/2)
      while %y.strlen% < %st%
        eval o .%o%
        eval y %o%
        eval m ?%m%
        eval p %m%
        if %dif% == %y.strlen%
          eval wid1 %p%
        end
      done
    end
    eval opt1 8 + %test%
    eval opt2 (2*%wid1.strlen%)+%sumslen.strlen%+5
    %echo% 	WWizzzzzzzzzz....	n
    if (%opt1%-2) == (%opt2%)
      %echo% 	c...%y%...	n
      %echo% 	c:	C..%y%..	c:	n
      %echo% 	c:	C:	G   %speech% 	C  :	c:	n
      %echo% 	c:	C:.%y%.:	c:	n
      %echo% 	c:	C: %wid1%	G %sum% 	C%wid1% :	c:	n
      %echo% 	c:	C:.%y%.:	c:	n
      %echo% 	c:..%y%..:	n
    else
      %echo% 	r....%y%...	n
      %echo% 	r:	R...%y%..	r:	n
      %echo% 	r:	R:	G    %speech% 	R  :	r:	n
      %echo% 	r:	R:..%y%.:	r:	n
      %echo% 	r:	R: %wid1%	G %sum% 	R%wid1% :	r:	n
      %echo% 	r:	R:..%y%.:	r:	n
      %echo% 	r:...%y%..:	n
    end
  end
end
~
#1202
TRAINING - Safe Wear Wield Event~
1 j 100
~
* TRAINING: safe wear/wield event demonstration.
* This example intentionally causes no damage and never purges the object.
%send% %actor% The training item gives a brief pulse as its wear trigger fires.
%echoaround% %actor% %actor.name% tests a scripted training item.
~
#1203
RETIRED - Legacy Speech Joke Demo~
0 d 100
*~
* RETIRED legacy speech-joke trigger. Intentionally inert in Adventurer's Lair.
~
#1204
RETIRED - Obsolete Portal Demo~
1 c 100
en~
* RETIRED obsolete portal demonstration. Intentionally inert; old R3001 teleport removed.
~
#1205
TRAINING - Inspect Object Affects~
1 n 100
~
* By Rumble of The Builder Academy    tbamud.com 9091
if %self.affects(BLIND)%
  %echo% This object is affected with blind.
end
if %self.affects(INVIS)%
  %echo% This object is affected with invisibility.
end
if %self.affects(DET-ALIGN)%
  %echo% This object is affected with detect alignment.
end
if %self.affects(DET-INVIS)%
  %echo% This object is affected with detect invisibility.
end
if %self.affects(DET-MAGIC)%
  %echo% This object is affected with detect magic.
end
if %self.affects(SENSE-LIFE)%
  %echo% This object is affected with sense life.
end
if %self.affects(WATWALK)%
  %echo% This object is affected with water walk.
end
if %self.affects(SANCT)%
  %echo% This object is affected with sanctuary.
end
if %self.affects(GROUP)%
  %echo% This object is affected with group.
end
if %self.affects(CURSE)%
  %echo% This object is affected with curse.
end
if %self.affects(INFRA)%
  %echo% This object is affected with infravision.
end
if %self.affects(POISON)%
  %echo% This object is affected with poison.
end
if %self.affects(PROT-EVIL)%
  %echo% This object is affected with protection from evil.
end
if %self.affects(PROT-GOOD)%
  %echo% This object is affected with protection from good.
end
if %self.affects(SLEEP)%
  %echo% This object is affected with sleep.
end
if %self.affects(NO_TRACK)%
  %echo% This object is affected with no track.
end
if %self.affects(FLYING)%
  %echo% This object is affected with flying.
end
if %self.affects(SCUBA)%
  %echo% This object is affected with scuba.
end
if %self.affects(SNEAK)%
  %echo% This object is affected with sneak.
end
if %self.affects(HIDE)%
  %echo% This object is affected with hide.
end
if %self.affects(UNUSED)%
  %echo% This object is affected with unused.
end
if %self.affects(CHARM)%
  %echo% This object is affected with charm.
end
~
#1206
RETIRED - Legacy Capture Logger~
2 c 0
*~
* RETIRED legacy capture/debug logger. Intentionally inert.
~
#1207
LAB - Random Ambient Sound Pattern~
2 b 2
~
eval forest_sounds %random.25%
switch %forest_sounds%
  case 1
    %echo% 	gThe gently chirping of crickets peacefully resonate across the forest.	n
  break
  case 2
    %echo% A haze of 	yfir	Wefl	yies	n dart in between some ancient cedars.
  break
  case 3
    %echo% 	DA thick fog drifts in, dampening the moss.	n
  break
  case 4
    %echo% 	DThe area is surrounded by a visually impeneratable mist.	n
  break
  case 5
    %echo% 	DThe gray haze starts to glow a dim silvery shade as the moonlight strikes it.	n
  break
  case 6
    %echo% 	DThe damp fallen clouds swirl slightly in an eddying wind.	n
  break
  case 7
    %echo% 	DThe thick brume shifts and ebbs away slightly.	n
  break
  case 8
    %echo% A hushed whispering sound seems to emanate from the patch of 	rt	wo	ra	wd	rs	wt	ro	wo	rl	ws	n.
  break
  case 9
    %echo% The largest 	rt	wo	ra	wd	rs	wt	ro	wo	rl	n yawns openly and mumbles something to one of its friends.
  break
  case 10
    %echo% Like a diminutive choir, the patch of 	rt	wo	ra	wd	rs	wt	ro	wo	rl	ws	n let loose a high-pitched song of peace.
  break
  case 11
    %echo% From the east, tiny voices talk amongst themselves in their own plant-like language.
  break
  case 12
    %echo% The patch of 	rt	wo	ra	wd	rs	wt	ro	wo	rl	ws	n sway synchronisingly in the silver moonlight.
  break
  case 13
    %echo% The peaceful chirping of bird-song floats down from above.
  break
  case 14
    %echo% 	gA piping little note sings down to you from above.	n
  break
  case 15
    %echo% The tweeting of a newly born bird calling to its mother echoes around the forest.
  break
  case 16
    %echo% The trill bird call of love emanates from the branches above.
  break
  case 17
    %echo% The sound of ruffling and the snapping of small twigs reaches your ears.
  break
  case 18
    %echo% 	gA rapid chattering drifts down from the giant trees to the northeast.	n
  break
  case 19
    %echo% With inequable grace, a snowy white owl ghosts in on silent wings.
  break
  case 20
    %echo% A 	dblack 	Dbat	n flutters across the forest, high above.
  break
  case 21
    %echo% 	gA relaxed nattering can be heard in the top of a tree to the south.	n
  break
  case 22
    %echo% A hedgehog slowly wanders in between some trees and out of view.
  break
  case 23
    %echo% A faint wind breathes in from all directions, steeping the mists.
  break
  case 24
    %echo% A 	rr	ya	bi	gn	cb	mo	rw	n-colored butterfly floats across the clearing.
  break
  case 25
    %echo% A strange 	Yglowing 	wluminescence	n drifts off to the north, fading into the damp fog.
  break
  default
  break
done
~
#1208
RETIRED - Legacy Variable Debug~
2 d 100
*~
* RETIRED legacy variable-debug trigger. Intentionally inert.
~
#1209
KEEP - Commons Hearthchair Sit~
1 c 100
si~
* KEEP: generic Commons Hearthchair sit behavior.
if %cmd.mudcommand% == sit && chair /= %arg%
%echoaround% %actor% %actor.name% settles into the hearthchair.
%send% %actor% You settle comfortably into the hearthchair.
%force% %actor% sit
else
return 0
end
~
#1210
TRAINING - Actor Preference Checks~
2 q 100
~
if %actor.pref(BRIEF)%
  %send% %actor% You have BRIEF on.
end
if %actor.pref(COMPACT)%
  %send% %actor% You have COMPACT on.
end
if %actor.pref(NO_SHOUT)%
  %send% %actor% You have NO_SHOUT on.
end
if %actor.pref(NO_TELL)%
  %send% %actor% You have NO_TELL on.
end
if %actor.pref(D_HP)%
  %send% %actor% You have D_HP on.
end
if %actor.pref(D_MANA)%
  %send% %actor% You have D_MANA on.
end
if %actor.pref(D_MOVE)%
  %send% %actor% You have D_MOVE on.
end
if %actor.pref(AUTOEX)%
  %send% %actor% You have AUTOEX on.
end
if %actor.pref(NO_HASS)%
  %send% %actor% You have NO_HASS on.
end
if %actor.pref(QUEST)%
  %send% %actor% You have QUEST on.
end
if %actor.pref(SUMN)%
  %send% %actor% You have SUMN on.
end
if %actor.pref(NO_REP)%
  %send% %actor% You have NO_REP on.
end
if %actor.pref(LIGHT)%
  %send% %actor% You have LIGHT on.
end
if %actor.pref(C1)%
  %send% %actor% You have C1 on.
end
if %actor.pref(NO_WIZ)%
  %send% %actor% You have NO_WIZ on.
end
if %actor.pref(L1)%
  %send% %actor% You have L1 on.
end
if %actor.pref(L2)%
  %send% %actor% You have L2 on.
end
if %actor.pref(NO_AUC)%
  %send% %actor% You have NO_AUC on.
end
if %actor.pref(NO_GOS)%
  %send% %actor% You have NO_GOS on.
end
if %actor.pref(RMFLG)%
  %send% %actor% You have RMFLG on.
end
if %actor.pref(D_AUTO)%
  %send% %actor% You have D_AUTO on.
end
if %actor.pref(CLS)%
  %send% %actor% You have CLS on.
end
if %actor.pref(BLDWLK)%
  %send% %actor% You have BLDWLK on.
end
if %actor.pref(AFK)%
  %send% %actor% You have AFK on.
end
if %actor.pref(AUTOLOOT)%
  %send% %actor% You have AUTOLOOT on.
end
if %actor.pref(AUTOGOLD)%
  %send% %actor% You have AUTOGOLD on.
end
if %actor.pref(AUTOSPLIT)%
  %send% %actor% You have AUTOSPLIT on.
end
if %actor.pref(AUTOSAC)%
  %send% %actor% You have AUTOSAC on.
end
if %actor.pref(AUTOASSIST)%
  %send% %actor% You have AUTOASSIST on.
end
~
#1211
LAB - Questpoint Variable Example~
2 b 100
~
* LAB: questpoint variable ACCESS example.
* Production training version is read-only and never changes questpoints.
set actor %random.char%
if %actor%
%echo% Questpoints on selected actor: %actor.questpoints%
end
~
#1212
KEEP - Animal Chase Movement~
1 c 4
go~
if %cmd% == go
  * By Mordecai
  * Animal chase board game. Triggers: 1212-1214. O1212, M1212 in R1200.
  if !(%arg%==up||%arg%==down||%arg%==left||%arg%==right||!%created%||!%arg%||%arg%==stay)
    return 0
    if %cmd%!=gos
      %send% %actor% Type: go < up | down | left | right >
    end
    halt
  end
  if %arg%
    eval dedu %nextlev%*105
    if %exx%>%dedu%
      nop %actor.exp(-%dedu%)%
      set dd %dedu%
    end
  end
  if !%nextlev%
    set nextlev 1
    global nextlev
  elseif %nextlev_s%
    eval nextlev %nextlev_s%
    global nextlev
  end
  if !%created%
    %force% %actor% createnewgame
  end
  if %created%
    set p_dir %arg%
    extract px 1 %playerco%
    extract py 2 %playerco%
    set [%py%][%px%] 	g\*	n
    switch %p_dir%
      case right
        if %px%!=10
          eval px %px%+1
        else
          eval px 1
        end
      break
      case left
        if %px%!=1
          eval px %px%-1
        else
          eval px 10
        end
      break
      case down
        if %py%!=1
          eval py %py%-1
        else
          eval py 10
        end
      break
      case up
        if %py%!=10
          eval py %py%+1
        else
          eval py 1
        end
      break
    done
    if %chase%>0
      eval chase %chase%-1
      global chase
    end
    if ([%py%][%px%]==%spec_prize%)
      set spec_prize
      global spec_prize
      eval exo (%nextlev%*2000)
      nop %actor.exp(%exo%)%
      eval points %points%+(%nextlev%*2)
      global points
      set alert You can now FLY and kill the animal!
      eval chase 5+(%nextlev%/70)+(%chase%)
      global chase
    end
    eval holech %%[%py%][%px%]%%
    if %holech%/=@@
      if !(%chase%>0)
        set alert 	MYou fall into a bottomless pit!	n
        unset points
        set nextlev 0
        global nextlev
        unset created
        unset exx
        set ax 1
        set ay 1
        set px 10
        set py 10
        %force% %actor% createnewgame
      else
        set alert You FLY over the pit!
      end
    end
    eval prz_ch %%[%py%][%px%]%%
    if (%prz_ch.contains(p)%)
      eval points (%points%+%nextlev%)
      global points
      eval exo (%nextlev%*950)
      nop %actor.exp(%exo%)%
      set [%py%][%px%] 	c.	n
      global [%py%][%px%]
      eval prize_count (%prize_count%)+1
      set winner [%prize_count%]
      if %prize_count%>=%numofprizes%
        eval nextlev (%nextlev%)+1
        global nextlev
        set winner 	YYou Win!!!	n Moving to level %nextlev%.
        set px 10
        set py 10
        set ax 1
        set ay 1
        set created
        global created
        set prize_count 0
        set numofprizes 0
        %force% %actor% createnewgame
      end
      global prize_count
      set alert You collect a prize. %winner%
    end
    set playerco %px% %py%
    global playerco
    if (%random.15%==1)||(%spec_on%)
      if !%spec_on%
        eval spec_on %random.4%+(%nextlev%/90)
        global spec_on
      end
      eval spec_on (%spec_on%)-1
      global spec_on
      if !%spec_prize%
        eval spec_prize [%random.10%][%random.10%]
        global spec_prize
      else
        set %spec_prize% 	yY	n
        eval spec_prize
        global spec_prize
      end
    end
    if %sizem%
      set [%py%][%px%] 	GO	n
      unset sizem
    else
      set [%py%][%px%] 	Go	n
      set sizem 1
      global sizem
    end
    extract ax 1 %animal%
    extract ay 2 %animal%
    set [%ay%][%ax%] 	r\*	n
    if !%dir_chosen%
      set dir_chosen 1
      if %px%>%ax%
        eval dis_x (%px%-%ax%)
        eval move_x 1
      elseif %px%<%ax%
        eval dis_x (%ax%-%px%)
        eval move_x 2
      end
      if %py%>%ay%
        eval dis_y (%py%-%ay%)
        eval move_y 4
      elseif %py%<%ay%
        eval dis_y (%ay%-%py%)
        eval move_y 3
      end
      if %dis_x%>%dis_y%
        set ani_dir %move_x%
      else %dis_x%<%dis_y%
        set ani_dir %move_y%
      end
      eval dificulty 100-(%nextlev%*4)
      if (%random.100%)<=(%dificulty%)
        eval ani_dir %random.4%
      end
      if %chase%
        if %ani_dir%==1
          eval ani_dir 2
        elseif %ani_dir%==2
          eval ani_dir 1
        elseif %ani_dir%==3
          eval ani_dir 4
        elseif %ani_dir%==4
          eval ani_dir 3
        end
      end
      if !%arg%
        set ani_dir 5
      end
      switch %ani_dir%
        case 1
          if %ax%!=10
            eval ax %ax%+1
          else
            eval ax 1
          end
        break
        case 2
          if %ax%!=1
            eval ax %ax%-1
          else
            eval ax 10
          end
        break
        case 3
          if %ay%!=1
            eval ay %ay%-1
          else
            eval ay 10
          end
        break
        case 4
          if %ay%!=10
            eval ay %ay%+1
          else
            eval ay 1
          end
        break
      done
      set animal %ax% %ay%
      global animal
    end
    eval [%ay%][%ax%] 	Ra	n
    eval ch_3 %%[%py%][%px%]%%
    set ch_3 %ch_3%
    if %ch_3.contains(a)%
      if %chase%
        eval pointinc %nextlev%*%random.10%
        eval points (%points%+%pointinc%)
        global points
        eval exo %nextlev%*7000
        nop %actor.exp(%exo%)%
        eval nextlev %nextlev%+1
        global nextlev
        set alert You kill the animal. You gain %pointinc% points and a new level. (%nextlev%)
        unset created
        set ax 1
        set ay 1
        set px 10
        set py 10
        %force% %actor% createnewgame
      else
        set alert You have been eaten by the 	Ranimal	n.
        set points 0
        global points
        set nextlev 0
        global nextlev
        unset created
        unset chase
        set exx 0
        global exx
        %force% %actor% createnewgame
      end
    end
    eval h 11
    while (%h%>1)
      eval h (%h%)-1
      eval printrow%h% %%[%h%][1]%% %%[%h%][2]%% %%[%h%][3]%% %%[%h%][4]%% %%[%h%][5]%% %%[%h%][6]%% %%[%h%][7]%% %%[%h%][8]%% %%[%h%][9]%% %%[%h%][10]%%
      *eval printrow%h% %%row%h%%%
    done
    if %numofprizes%<10
      set numop 0%numofprizes%
    else
      set numop %numofprizes%
    end
    if %prize_count%<10
      set przc 0%prize_count%
    else
      set przc %prize_count%
    end
    if %nextlev%<10
      set levlev 000%nextlev%
    elseif %nextlev%<100
      set levlev 00%nextlev%
    elseif %nextlev%<1000
      set levlev 0%nextlev%
    else
      set levlev %nextlev%
    end
    eval exx (%exo%+%exx%)-(%dd%)
    if %exx%<=0
      set exx 0
    end
    global exx
    eval cht %chase%-1
    if %cht%>0
      set chy You can FLY and chase the animal %cht% more times.
    end
    set snd %send% %actor% 	n
    %force% %actor% cls
    %snd%                    )       \\   /      (
    %snd%                   /\|\\      )\\_/(     /\|\\
    %snd% \*                / \| \\    (/\\\|/\\)   / \| \\         \*
    %snd% \|\`._____________/__\|__o____\\\`\|'/___o__\|__\\______.'\|
    %snd% \|                    '\^\` \|  \\\|/   '\^\`             \|
    %snd% \|                        \|   V   level: %levlev%      \| Dir: 	C %arg%	n
    %snd% \|   %printrow10%  \| 	M@@	n = Bottomless Pit     \| Points: 	Y%points%	n
    %snd% \|   %printrow9%  \| 	yY	n = Power Up           \| Exp: 	C%exx%	n
    %snd% \|   %printrow8%  \| 	Wp	n = Prize              \| AMU: %dedu%
    %snd% \|   %printrow7%  \| 	Go	n = You                \|
    %snd% \|   %printrow6%  \| 	Ra	n = Animal             \|
    %snd% \|   %printrow5%  \| 	cTo Move Type:	n          \|
    %snd% \|   %printrow4%  \| 	Cgo <up\|down\|left\|right>	n\|
    %snd% \|   %printrow3%  \|                        \|
    %snd% \|   %printrow2%  \| 	BCost of AMU exp per MV	n \|
    %snd% \|   %printrow1%  \|    	BIf you have exp.	n    \|
    %snd% \|                        \|                        \|
    %snd% \|                        \|  NEEDED: 	R%numop%	n HAVE: 	G%przc%	n   \|
    %snd% \| .______________________\|______________________. \|
    %snd% \|'         l    /\\ /     \\\\            \\ /\\    l \`\|
    %snd% \*          l  /   V       ))             V  \\  l  \*
    %snd%            l/            //                   \\I
    %snd%                          V
    %snd%  %alert%
    %snd%  %chy%
    %snd%  Animal: ax:%ax% ay:%ay% You: px:%px% py:%py%
  end
else
  return 0
end
~
#1213
KEEP - Animal Chase New Game~
1 c 100
newgame~
* By Mordecai
* Animal chase board game. Triggers: 1212-1214. O1212, M1212 in R1200.
if %arg.cdr% == %actor.id%
  eval nextlev %arg.car%
  global nextlev
end
if %nextlev%>1&&%points%>0
  set dd scoreboardmob
  if %dd.vnum%>0
    set nums 1
    while %nums%
      eval j %j%+1
      eval nums %dd.varexists(%j%)%
      if %nums%
        eval nums2 %%dd.%j%%%
        if (%nums2%/=%actor.name%)
          extract partlev 2 %nums2%
          set added 1
          if %partlev%<%nextlev%
            set %j% %actor.name% %nextlev% %points% %exx%
            remote %j% %dd.id%
          end
        end
      else
        if !%added%
          set %j% %actor.name% %nextlev% %points% %exx%
          remote %j% %dd.id%
        end
      end
    done
  end
end
unset chase
set tail 1
global tail
eval animal 1 1
global animal
eval playerco 10 10
global playerco
set [1][1] 	ra	n
set [10][10] 	Go	n
global [1][1]
global [10][10]
eval numofprizes 0
global numofprizes
set prize_count 0
global prize_count
set created 1
global created
eval ww 11
while (%ww%>1)
  eval ww %ww%-1
  eval n %ww%
  eval row %random.10%
  eval r_col (%random.3%-1)
  set [%n%][1] 	c.	n
  global [%n%][1]
  set [%n%][2] 	c.	n
  global [%n%][2]
  set [%n%][3] 	c.	n
  global [%n%][3]
  set [%n%][4] 	c.	n
  global [%n%][4]
  set [%n%][5] 	c.	n
  global [%n%][5]
  set [%n%][6] 	c.	n
  global [%n%][6]
  set [%n%][7] 	c.	n
  global [%n%][7]
  set [%n%][8] 	c.	n
  global [%n%][8]
  set [%n%][9] 	c.	n
  global [%n%][9]
  set [%n%][10] 	c.	n
  global [%n%][10]
  if (%r_col%)
    while (%r_col%<3)
      eval r_col %r_col%+1
      eval jj %random.10%
      if !(%posis%/=[%n%][%jj%])
        eval numofprizes %numofprizes%+1
        global numofprizes
        eval [%n%][%jj%] 	Wp	n
        global [%n%][%jj%]
        set posis %posis% [%n%][%jj%]
      end
    done
  end
done
eval r_ttt %nextlev%-35
if (%r_ttt%>0)
  eval hrt (%random.6%-1)+(%nextlev%/20)
  while %hrt%>0
    eval hrt %hrt%-1
    eval j8 %random.10%
    eval j9 %random.10%
    eval cer %%[%j9%][%j8%]%%
    if !(%cer%/=p)
      eval [%j9%][%j8%] 	M\@@	n
      global [%j9%][%j8%]
    end
  done
end
if !(%[10][10]%/=p)
  set [10][10] 	c.	n
  global [10][10]
end
~
#1214
KEEP - Animal Chase Scoreboard~
0 d 100
score scores~
* By Mordecai
* Animal chase board game. Triggers: 1212-1214. O1212, M1212 in R1200.
set j 1
while %self.varexists(%j%)%
  eval r %%self.%j%%%
  eval nam %r.car%
  eval k %nam.strlen%
  while %k%<15
    eval k %k%+1
    eval sgg %%s%j%%%
    set s%j% %sgg%-
  done
  eval j %j%+1
done
%echo% 	yO===============SCORE======BOARD=====================O	n
wait 1
%echo% O=#==NAME============\|=Level=\|=Points==\|=EXP=========O
set i 1
set j 1
while %self.varexists(%j%)%
  eval r %%self.%j%%%
  eval nam %r.car%
  extract ll 2 %r%
  extract points 3 %r%
  extract exp 4 %r%
  eval sp %%s%j%%%
  if %ll%<10
    set ll 0000%ll%
  elseif %ll%<100
    set ll 000%ll%
  elseif %ll%<1000
    set ll 00%ll%
  elseif %ll%<10000
    set ll 0%ll%
  end
  if %points%<10
    set points 000000%points%
  elseif %points%<100
    set points 00000%points%
  elseif %points%<1000
    set points 0000%points%
  elseif %points%<10000
    set points 000%points%
  elseif %points%<100000
    set points 00%points%
  elseif %points%<1000000
    set points 0%points%
  end
  eval d %j%
  if %d%<9
    set d 0%j%
  end
  %echo% \|	g%d%	n: 	w%nam%	n %sp%\| 	c%ll%	n \| 	y%points%	n \| 	W%exp%	n
  eval j %j%+1
done
%echo% O=#==================================================O
~
#1215
TRAINING - Hunger Thirst Drunk Check~
2 g 100
~
wait 1
%echo% Hello %actor.name%
%echo% Hunger: %actor.hunger%   Thirst: %actor.thirst%   Drunk: %actor.drunk%
nop %actor.hunger(50)%
nop %actor.thirst(50)%
nop %actor.drunk(50)%
%echo% Hunger: %actor.hunger%   Thirst: %actor.thirst%   Drunk: %actor.drunk%
nop %actor.hunger(-10)%
nop %actor.thirst(-10)%
nop %actor.drunk(-10)%
%echo% Hunger: %actor.hunger%   Thirst: %actor.thirst%   Drunk: %actor.drunk%
nop %actor.hunger(20)%
nop %actor.thirst(21)%
nop %actor.drunk(22)%
%echo% Hunger: %actor.hunger%   Thirst: %actor.thirst%   Drunk: %actor.drunk%
*
while %actor.hunger% >= 0
  nop %actor.hunger(-1)%
done
while %actor.thirst% >= 0
  nop %actor.thirst(-1)%
done
while %actor.drunk% >= 0
  nop %actor.drunk(-1)%
done
~
#1216
RETIRED - Legacy Crash Get Test~
1 g 100
~
* RETIRED legacy crash-test trigger. Intentionally inert.
~
#1217
RETIRED - Unclassified Legacy Enter Test~
2 g 100
~
* RETIRED unclassified legacy enter test. Intentionally inert.
~
#1218
TRAINING - Multi Command Parsing~
2 c 100
t~
if %cmd% == test
  * Careful not to use Arguments * or this trig will freeze you.
  * set the first arg
  set command %arg.car%
  * set the rest of the arg string
  set therest %arg.cdr%
  * while there is an arg keep going
  while %command%
    %echo% the first arg is: %command%
    %echo% the remaining arg is: %therest%
    set command %therest.car%
    set therest %therest.cdr%
  done
end
~
#1220
LAB - Room Flag Inspection~
2 g 100
~
if %self.roomflag(DARK)%
  %echo% This is a dark room.
end
if %self.roomflag(DEATH)%
  %echo% This is a death trap - goodbye!
end
if %self.roomflag(NO_MOB)%
  %echo% Mobiles cannot enter this room.
end
if %self.roomflag(INDOORS)%
  %echo% This room is indoors.
end
if %self.roomflag(PEACEFUL)%
  %echo% You can't kill anything in this room.
end
if %self.roomflag(NO_TRACK)%
  %echo% You cannot track anything through this room.
end
if %self.roomflag(NO_MAGIC)%
  %echo% You cannot cast spells in here!
end
if %self.roomflag(TUNNEL)%
  %echo% This room is a narrow tunnel.
end
if %self.roomflag(PRIVATE)%
  %echo% This is a private room.
end
if %self.roomflag(GODROOM)%
  %echo% Only Gods can enter this room.
end
if %self.roomflag(HOUSE)%
  %echo% This is a house.
end
if %self.roomflag(HCRSH)%
  %echo% This is a house which will crash-save.
end
if %self.roomflag(ATRIUM)%
  %echo% This is an atrium for a house.
end
~
#1221
RETIRED - Unclassified Command Test~
2 c 100
*~
* RETIRED unclassified legacy command test. Intentionally inert.
~
#1222
RETIRED - Abandoned Random Object Test~
1 b 100
~
* RETIRED abandoned random-object test. Intentionally inert.
~
#1233
RETIRED - Personal Leave Test~
2 q 100
~
* RETIRED personal legacy leave test. Intentionally inert.
~
#1267
LAB - Secret Drawer Command~
1 c 4
look~
if %arg% == drawer
  %purge% drawer
  %load% obj 7711
  %echo% The small drawer appears to be nothing more than a mere crack underneath the
  %echo% desk.  The only thing that gives it away is the small keyhole that winks at you
  %echo% upon closer inspection.
else
  return 0
end
~
#1268
RETIRED - Obsolete Personal Autolook~
2 g 100
~
* RETIRED obsolete personal-room autolook. Intentionally inert.
~
#1269
RETIRED - Personal Harp Speech~
0 d 100
play~
* RETIRED personal harp-response trigger. Intentionally inert.
~
#1270
TRAINING - Safe Wear Trigger Switch~
1 j 100
~
* TRAINING: safe object Wear trigger example.
* Legacy version purged the item and dealt 2020 damage; both were removed.
wait 1
%send% %actor% The training switch clicks softly as its wear trigger fires.
%echoaround% %actor% The training switch carried by %actor.name% gives a quiet click.
~
#1271
RETIRED - Transform Crash Test~
0 g 100
~
* RETIRED transform/crash test. Intentionally inert; transform removed.
~
#1280
RETIRED - Dangerous City Raid Demo~
2 b 100
~
* RETIRED dangerous city-raid demonstration.
* Legacy body could load roughly 100 mobs across Midgaard and force global gossip.
* Preserved as an inert VNUM so old references remain resolvable.
~
#1282
LAB - Teleport Enter Pattern~
2 g 100
~
%echo% %self.name% squints at ~%actor.name%  asdf'
%echo% buh-bye
%teleport% %actor% 3
~
#1283
TRAINING - Deal Single Card~
1 c 7
deal~
switch %random.4%
  case 1
    set col
    set suit Diamond
  break
  case 2
    set col
    set suit Heart
  break
  case 3
    set col
    set suit Club
  break
  case 4
    set col
    set suit Spade
  break
  default
    set suit JOKER!
  break
done
%echo% suit generated = %suit%
*
set r %random.13%
if %r% == 1
  set rank Ace
elseif %r% == 11
  set rank Jack
elseif %r% == 12
  set rank Queen
elseif %r% == 13
  set rank King
else
  eval rank %r%
end
%echo% ranks generated = %rank%
%echo% should check if card %rank%%suit% exists now.
%echo% (%rank%%suit%) %%%rank%%suit%%% (%%%rank%%suit%%%) (%%rank%%suit%%) (%rank%%suit%) %(%rank%%suit%)%
eval thecard %%%rank%%suit%%%
set thecardset %%%rank%%suit%%%
%echo% %thecard% %thecardset%
if %thecard% == 1
  %echo% Should deal a card
  %echo% %col%%rank% of %suit%
  set %rank%%suit% 0
  global %rank%%suit%
  eval deck %deck% -1
  global deck
end
~
#1284
TRAINING - Shuffle Deck State~
1 c 7
*~
if %cmd% == shuffle
  %echo% %deck% cards in the deck.
  set deck 52
  global deck
  *
  set Ace_Spades 1
  global Ace_Spades
  set 2_Spades 1
  global 2_Spades
  set 3_Spades 1
  global 3_Spades
  set 4_Spades 1
  global 4_Spades
  set 5_Spades 1
  global 5_Spades
  set 6_Spades 1
  global 6_Spades
  set 7_Spades 1
  global 7_Spades
  set 8_Spades 1
  global 8_Spades
  set 9_Spades 1
  global 9_Spades
  set 10_Spades 1
  global 10_spades
  set Jack_Spades 1
  global Jack_Spades
  set Queen_Spades 1
  global Queen_Spades
  set King_Spades 1
  global King_Spades
  *
  set Ace_Hearts 1
  global Ace_Hearts
  set 2_Hearts 1
  global 2_Hearts
  set 3_Hearts 1
  global 3_Hearts
  set 4_Hearts 1
  global 4_Hearts
  set 5_Hearts 1
  global 5_Hearts
  set 6_Hearts 1
  global 6_Hearts
  set 7_Hearts 1
  global 7_Hearts
  set 8_Hearts 1
  global 8_Hearts
  set 9_Hearts 1
  global 9_Hearts
  set 10_Hearts 1
  global 10_Hearts
  set Jack_Hearts 1
  global Jack_Hearts
  set Queen_Hearts 1
  global Queen_Hearts
  set King_Hearts 1
  global King_Hearts
  *
  set Ace_Clubs 1
  global Ace_Clubs
  set 2_Clubs 1
  global 2_Clubs
  set 3_Clubs 1
  global 3_Clubs
  set 4_Clubs 1
  global 4_Clubs
  set 5_Clubs 1
  global 5_Clubs
  set 6_Clubs 1
  global 6_Clubs
  set 7_Clubs 1
  global 7_Clubs
  set 8_Clubs 1
  global 8_Clubs
  set 9_Clubs 1
  global 9_Clubs
  set 10_Clubs 1
  global 10_Clubs
  set Jack_Clubs 1
  global Jack_Clubs
  set Queen_Clubs 1
  global Queen_Clubs
  set King_Clubs 1
  global King_Clubs
  *
  set Ace_Diamonds 1
  global Ace_Diamonds
  set 2_Diamonds 1
  global 2_Diamonds
  set 3_Diamonds 1
  global 3_Diamonds
  set 4_Diamonds 1
  global 4_Diamonds
  set 5_Diamonds 1
  global 5_Diamonds
  set 6_Diamonds 1
  global 6_Diamonds
  set 7_Diamonds 1
  global 7_Diamonds
  set 8_Diamonds 1
  global 8_Diamonds
  set 9_Diamonds 1
  global 9_Diamonds
  set 10_Diamonds 1
  global 10_Diamonds
  set Jack_Diamonds 1
  global Jack_Diamonds
  set Queen_Diamonds 1
  global Queen_Diamonds
  set King_Diamonds 1
  global King_Diamonds
  *
  %echo% %actor.name% shuffles %actor.hisher% deck.
  %echo% %deck% cards in the deck.
  *
elseif %cmd% == deal
  *   while (%deck%)
  %echo% while begins.
  switch %random.4%
    case 1
      set col
      set suit Diamonds
    break
    case 2
      set col
      set suit Hearts
    break
    case 3
      set col
      set suit Clubs
    break
    case 4
      set col
      set suit Spades
    break
    default
      set suit JOKER!
    break
  done
  %echo% suit generated = %suit%
  *
  set r %random.13%
  if %r% == 1
    set rank Ace
  elseif %r% == 11
    set rank Jack
  elseif %r% == 12
    set rank Queen
  elseif %r% == 13
    set rank King
  else
    eval rank %r%
  end
  %echo% ranks generated = %rank%
  %echo% should check if card %rank%%suit% exists now.
  eval thecard %%%rank%_%suit%%%
  %echo% %thecard%
  if %thecard% == 1
    %echo% Should deal a card
    %echo% %col%%rank% of %suit%
    set %rank%_%suit% 0
    global %rank%_%suit%
    eval deck %deck% -1
    global deck
  end
  * %echo% %col%%rank% of %suit%
  * set %rank%_%suit% 0
  * global %rank%_%suit%
  * eval deck %deck% -1
  * global deck
  *done
else
  return 0
end
~
#1285
TRAINING - Safe Damage Math~
2 g 100
~
* TRAINING: safe damage-math example.
* Computes a random amount up to half current HP but DOES NOT apply damage.
eval num_hitp %actor.hitp%/2
if %num_hitp% > 0
eval rx %%random.%num_hitp%%%
%send% %actor% Training calculation: a live damage trigger could have selected %rx% damage here.
end
~
#1286
KEEP - Commons Hearthchair Sleep~
1 c 100
sl~
* KEEP: generic Commons Hearthchair sleep behavior.
%echoaround% %actor% %actor.name% settles into the hearthchair and drifts toward sleep.
%send% %actor% The warmth of the hearthchair makes it easy to rest.
%force% %actor% sleep
~
#1287
RETIRED - Legacy Speech Global Debug~
0 d 100
test~
* RETIRED legacy speech/global debug trigger. Intentionally inert.
~
#1288
TRAINING - Delayed Hearth Ambience~
2 b 100
~
set fire %random.900%
wait %fire% sec
%echo% 	bThe fire crackles softly in the fireplace.	n
~
#1289
TRAINING - Random Room Ambience~
2 b 1
~
set office_noises %random.5%
switch %office_noises%
  case 1
    %echo% 	bLoud footsteps can be heard coming from the hall outside.	n
  break
  case 2
    %echo% 	bThe sound of thunder echoes in from outside.	n
  break
  case 3
    %echo% 	bA large book falls off the desk, hitting the floor with a loud thud.	n
  break
  case 4
    %echo% 	bTalking can be heard coming from outside the door.	n
  break
  case 5
    %echo% 	bThe sound of chirping birds flows in though the window.	n
  break
  default
  break
done
~
#1290
TRAINING - Actor Equipment Lookup~
0 g 100
~
if !%actor.eq(*)%
  say you are wearing nothing!
else
  say you are wearing something.
end
~
#1291
TRAINING - Boolean Conditions~
0 j 100
~
say you're %actor.name%!
say your vnum is %actor.vnum%
~
#1292
RETIRED - Legacy Find Crash Test~
2 g 100
~
* RETIRED legacy find/crash test. Intentionally inert.
~
#1293
RETIRED - Legacy Bribe Crash Dummy~
0 m 100
~
* RETIRED legacy bribe crash dummy. Intentionally inert.
~
#1294
RETIRED - Unclassified Speech Test~
0 d 100
heh~
* RETIRED unclassified legacy speech test. Intentionally inert.
~
#1295
LAB - Seasonal Login Example~
2 s 100
~
* By Rumble of The Builder Academy    tbamud.com 9091
if !%actor.has_item(222)%
  wait 1 s
  %echo% Santa bellows, 'Merry Christmas'
  %load% obj 222 %actor% inv
end
~
#1296
TRAINING - Random Equipment Example~
0 n 100
~
switch %random.5%
  case 1
    %load% obj 3010
    wield dagger
  break
  case 2
    %load% obj 3011
    wield sword
  break
  case 3
    %load% obj 3012
    wield club
  break
  case 4
    %load% obj 3013
    wield mace
  break
  case 5
    %load% obj 3014
    wield sword
  break
  default
    * this should be here, even if it's never reached
  break
done
~
#1297
LAB - Persistent Music State Example~
1 c 1
*~
*Originally written by someone, later rewritten by Fizban
*of The Builder Academy    tbamud.com 9091
switch %cmd%
  case StartMusic
    if %musicplaying% == 1
      %send% %actor% You are already playing music!
      halt
    end
    set musicplaying 1
    global musicplaying
    %send% %actor% You start playing guitar.
    %echoaround% %actor% %actor.name% starts playing guitar.
    wait 2 s
    if !%preferred_flourish%
      set flourish 3
    else
      set flourish %preferred_flourish%
    end
    set i 0
    while %musicplaying% == 1 && %i% < 100
      switch %flourish%
        case 1
          set flourish a wicked guitar solo.
        break
        case 2
          set flourish a chorus riff.
        break
        default
          set flourish a steady rhythm.
        break
      done
      %echoaround% %actor% %actor.name% performs %flourish%
      %send% %actor% You perform %flourish%
      if !%preferred_flourish%
        set flourish %random.5%
      end
      wait 10 s
    done
  break
  case PlaySolo
    set preferred_flourish 1
    global preferred_flourish
  break
  case PlayChorus
    set preferred_flourish 2
    global preferred_flourish
  break
  case PlayVerse
    set preferred_flourish 3
    global preferred_flourish
  break
  default
    return 0
  break
done
~
#1298
TRAINING - Receive Object Loader~
0 j 100
~
context %actor.id%
say object vnum: %object.vnum%
set answer_yes say Yes, I want that object :)
set answer_no say I already have that object !
set answer_reward say There you go. Here's an object for you. Thanks!
if (%object.vnum% == 1301)
  if (%zone_12_object1%)
    %answer_no%
    return 0
  else
    %answer_yes%
    set zone_12_object1 1
    global zone_12_object1
  end
elseif (%object.vnum% == 1302)
  if (%zone_12_object2%)
    %answer_no%
    return 0
  else
    %answer_yes%
    set zone_12_object2 1
    global zone_12_object2
  end
elseif (%object.vnum% == 1303)
  if (%zone_12_object3%)
    %answer_no%
    return 0
  else
    %answer_yes%
    set zone_12_object3 1
    global zone_12_object3
  end
elseif (%object.vnum% == 1304)
  if (%zone_12_object4%)
    %answer_no%
    return 0
  else
    %answer_yes%
    set zone_12_object4 1
    global zone_12_object4
  end
else
  say I do not want that object!
  return 0
end
if (%zone_12_object1% && %zone_12_object2% && %zone_12_object3% && %zone_12_object4%)
  %answer_reward%
  eval zone_12_reward_number %actor.zone_12_reward_number%+1
  * cap this to a max of 12 rewards.
  if %zone_12_reward_number%>12
    set zone_12_reward_number 12
  end
  remote zone_12_reward_number %actor.id%
  *  make sure all objects from 3016 and upwards have 'reward' as an alias
  eval loadnum 3015+%zone_12_reward_number%
  %load% o %loadnum%
  give reward %actor.name%
  unset zone_12_object1
  unset zone_12_object2
  unset zone_12_object3
  unset zone_12_object4
end
test
~
#1299
LAB - Stop Persistent Music State~
2 c 100
StopMusic~
*Works with Script 1297
*Written by Fizban of The Builder Academy tbamud.com 9091
if !%musicplaying%
  %send% %actor% You are not currently playing music.
  halt
end
unset flourish
unset musicplaying
%send% %actor% You stop playing music.
%echoaround% %actor% %actor.name% stops playing music.
%force% %actor% bow
detach 1297 %self.id%
~
$~
