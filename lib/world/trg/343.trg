#34300
RETIRED - Legacy Mascot Routine~
0 b 100
~
* RETIRED legacy mascot routine. Intentionally inert.
~
#34301
KEEP - Refreshment Steward Greeting~
0 hi 100
~
wait 2
say Welcome, %actor.name%. The refreshment alcove is open to staff on duty.
say Cups and provisions are kept in the cabinet; use 'list' if you need the available refreshments.
~
#34302
RETIRED - Legacy Drunken Spirit Routine~
0 b 100
~
* RETIRED obsolete drunken-spirit routine. Intentionally inert.
~
#34303
KEEP - Staff Access Reminder~
0 gi 100
~
if %actor.level% > 30
say Welcome, %actor.name%. Please use only the offices and tools appropriate to your assigned duties.
else
say This is a staff operations area. Please pass through only when directed and do not disturb restricted workspaces.
end
~
#34399
KEEP - Staff Aid Enchantments~
0 g 100
~
say You look worn, %actor.name%. Hold still and I will help you recover before you return to duty.
dg_cast 'heal' %actor.name%
wait 2
dg_cast 'sanctuary' %actor.name%
wait 2
dg_cast 'fly' %actor.name%
say There. Continue carefully.
~
$~
