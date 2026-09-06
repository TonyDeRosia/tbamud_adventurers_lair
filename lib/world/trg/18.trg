#1800
WARDWOOD - Trespasser Challenge~
0 g 100
~
if %actor.is_pc%
  emote turns toward %actor.name% as the surrounding roots tighten.
  %send% %actor% 	GThe Wardwood has noticed you.	n
  wait 1 sec
  mkill %actor%
end
~
#1801
WARDWOOD - Living Forest Combat~
0 k 28
~
if %actor.is_pc%
  switch %random.4%
    case 1
      emote drives a mass of roots through the soil toward %actor.name%.
    break
    case 2
      emote shudders as nearby branches bend toward the fight.
    break
    case 3
      %send% %actor% 	gThe forest floor heaves beneath your feet.	n
    break
    default
      emote lashes out with a splintering branch.
    break
  done
end
~
#1802
ABERRANT WOODMAGE - Challenge~
0 g 100
~
if %actor.is_pc%
  say The Sanctum cast me out. The forest did not.
  emote flexes a branchlike hand as blue light leaks through his bark.
  wait 1 sec
  mkill %actor%
end
~
#1803
ABERRANT WOODMAGE - Combat~
0 k 45
~
if %actor.is_pc%
  switch %random.3%
  case 1
  dg_cast 'color spray' %actor%
  break
  case 2
  dg_cast 'blindness' %actor%
  break
  default
  emote drives thorned roots across the ground toward %actor.name%.
  break
  done
end
~
#1804
SAPPHIRE THORN - Challenge~
0 g 100
~
if %actor.is_pc%
  emote unfolds a ring of hooked vines around its luminous blossom.
  %send% %actor% 	CCold sapphire pollen fills the air.	n
  wait 1 sec
  mkill %actor%
end
~
#1805
SAPPHIRE THORN - Combat~
0 k 42
~
if %actor.is_pc%
  switch %random.3%
  case 1
  dg_cast 'poison' %actor%
  break
  case 2
  dg_cast 'color spray' %actor%
  break
  default
  emote snaps a crown of thorns toward %actor.name%.
  break
  done
end
~
#1806
ROOTTHANE BELOVAR - Challenge~
0 g 100
~
if %actor.is_pc%
  say Turn back, little wanderer. I was old when your road was young.
  emote rises from his root-throne with a long wooden groan.
  wait 1 sec
  mkill %actor%
end
~
#1807
ROOTTHANE BELOVAR - Combat~
0 k 38
~
if %actor.is_pc%
  switch %random.3%
  case 1
  emote slams both rootbound fists into the earth.
  break
  case 2
  dg_cast 'curse' %actor%
  break
  default
  say The Wardwood remembers every wound.
  break
  done
end
~
#1808
OREN VALEC - Challenge~
0 g 100
~
if %actor.is_pc%
  say You crossed my forest, broke my wards, and entered my hall uninvited.
  say At least try to make the interruption educational.
  wait 1 sec
  mkill %actor%
end
~
#1809
OREN VALEC - Combat~
0 k 45
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'magic missile' %actor%
  break
  case 2
  dg_cast 'color spray' %actor%
  break
  case 3
  emote closes one hand, and loose stone rockets toward %actor.name%.
  break
  default
  dg_cast 'blindness' %actor%
  break
  done
end
~
#1810
ILYRA GLASS-TONGUED - Challenge~
0 g 100
~
if %actor.is_pc%
  say Which of me did you intend to challenge?
  emote smiles from three different reflections at once.
  wait 1 sec
  mkill %actor%
end
~
#1811
ILYRA GLASS-TONGUED - Combat~
0 k 50
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'blindness' %actor%
  break
  case 2
  dg_cast 'color spray' %actor%
  break
  case 3
  say You are fighting the wrong one.
  break
  default
  dg_cast 'magic missile' %actor%
  break
  done
end
~
#1812
THAROS EMBERHAND - Challenge~
0 g 100
~
if %actor.is_pc%
  say No lecture today. Demonstration only.
  emote closes his fist around a blue-white flame.
  wait 1 sec
  mkill %actor%
end
~
#1813
THAROS EMBERHAND - Combat~
0 k 48
~
if %actor.is_pc%
  switch %random.3%
  case 1
  dg_cast 'fireball' %actor%
  break
  case 2
  dg_cast 'burning hands' %actor%
  break
  default
  emote sweeps a sheet of sapphire flame across the chamber.
  break
  done
end
~
#1814
MOTHER VAELUNE - Challenge~
0 g 100
~
if %actor.is_pc%
  say I can already see the wound you are about to receive.
  say I would prefer that you leave before I must treat it.
  wait 1 sec
  mkill %actor%
end
~
#1815
MOTHER VAELUNE - Combat~
0 k 42
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'harm' %actor%
  break
  case 2
  dg_cast 'blindness' %actor%
  break
  case 3
  dg_cast 'heal' %self%
  break
  default
  say The stars warned you.
  break
  done
end
~
#1816
SARITH NOCTURNE - Challenge~
0 g 100
~
if %actor.is_pc%
  say Light makes such confident promises.
  emote lets his shadow reach toward %actor.name% before he moves.
  wait 1 sec
  mkill %actor%
end
~
#1817
SARITH NOCTURNE - Combat~
0 k 50
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'chill touch' %actor%
  break
  case 2
  dg_cast 'curse' %actor%
  break
  case 3
  dg_cast 'blindness' %actor%
  break
  default
  emote disappears into his own shadow for a heartbeat.
  break
  done
end
~
#1818
MASTER AERION - Challenge~
0 g 100
~
if %actor.is_pc%
  say Hear that? The storm has decided you are interesting.
  emote raises one hand as static crawls over the sapphire walls.
  wait 1 sec
  mkill %actor%
end
~
#1819
MASTER AERION - Combat~
0 k 52
~
if %actor.is_pc%
  switch %random.3%
  case 1
  dg_cast 'lightning bolt' %actor%
  break
  case 2
  dg_cast 'call lightning' %actor%
  break
  default
  emote tears a crack of thunder through the chamber.
  break
  done
end
~
#1820
SAPPHIRE GOLEM - Challenge~
0 g 100
~
if %actor.is_pc%
  emote turns its chest-core toward %actor.name%.
  %send% %actor% 	CWARD STATUS: INTRUDER.	n
  wait 1 sec
  mkill %actor%
end
~
#1821
SAPPHIRE GOLEM - Combat~
0 k 38
~
if %actor.is_pc%
  switch %random.3%
  case 1
  emote drives a stone fist down with crushing force.
  break
  case 2
  dg_cast 'color spray' %actor%
  break
  default
  emote sends its rotating runes flaring hard white.
  break
  done
end
~
#1822
ARCHIVIST MERIDANE - Challenge~
0 g 100
~
if %actor.is_pc%
  say You are loud, armed, and not on the appointment ledger.
  emote calmly closes the book floating before her.
  wait 1 sec
  mkill %actor%
end
~
#1823
ARCHIVIST MERIDANE - Combat~
0 k 45
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'magic missile' %actor%
  break
  case 2
  dg_cast 'curse' %actor%
  break
  case 3
  say Shhh.
  break
  default
  dg_cast 'color spray' %actor%
  break
  done
end
~
#1824
RUNE-BOUND THEURGE - Challenge~
0 g 100
~
if %actor.is_pc%
  say Nine seals bar the inner Sanctum. I am the tenth.
  emote snaps his revolving runes into a single blazing pattern.
  wait 1 sec
  mkill %actor%
end
~
#1825
RUNE-BOUND THEURGE - Combat~
0 k 55
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'fireball' %actor%
  break
  case 2
  dg_cast 'lightning bolt' %actor%
  break
  case 3
  dg_cast 'curse' %actor%
  break
  default
  dg_cast 'color spray' %actor%
  break
  done
end
~
#1826
ARCANE MANIFESTATION - Challenge~
0 g 100
~
if %actor.is_pc%
  %send% %actor% 	CThe living spell rearranges itself around your presence.	n
  emote folds into a sharp humanoid outline.
  wait 1 sec
  mkill %actor%
end
~
#1827
ARCANE MANIFESTATION - Combat~
0 k 60
~
if %actor.is_pc%
  switch %random.4%
  case 1
  dg_cast 'magic missile' %actor%
  break
  case 2
  dg_cast 'fireball' %actor%
  break
  case 3
  dg_cast 'lightning bolt' %actor%
  break
  default
  dg_cast 'color spray' %actor%
  break
  done
end
~
#1828
ARCHWIZARD CAELVARIS - Challenge~
0 g 100
~
if %actor.is_pc%
  say You have crossed the Wardwood and inconvenienced nearly everyone I employ.
  say That earns you an audience. Surviving it would earn you more.
  emote gestures once, and the great sapphire above the dais begins to turn.
  wait 1 sec
  mkill %actor%
end
~
#1829
ARCHWIZARD CAELVARIS - Combat~
0 k 65
~
if %actor.is_pc%
  switch %random.5%
  case 1
  dg_cast 'fireball' %actor%
  break
  case 2
  dg_cast 'lightning bolt' %actor%
  break
  case 3
  dg_cast 'harm' %actor%
  break
  case 4
  dg_cast 'blindness' %actor%
  break
  default
  dg_cast 'color spray' %actor%
  break
  done
end
~
$~
