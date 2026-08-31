#16453
Verdant Plains - Wisp Blinding Flare~
0 k 8
~
dg_cast 'blindness' %actor%
~
#16454
Verdant Plains - Sylph Windward Step~
0 k 12
~
dg_cast 'evasion'
~
#16455
Verdant Plains - Briar Snare~
0 k 12
~
dg_cast 'web' %actor%
~
#16456
Verdant Plains - Thornhide Charge~
0 k 12
~
bash %actor%
~
#16457
Verdant Plains - Rootbound Cycle~
0 k 15
~
eval rootroll %random.100%
if %rootroll% < 55
  dg_cast 'web' %actor%
else
  dg_cast 'barkskin'
end
~
#16458
Verdant Plains - Bloom Renewal~
0 k 10
~
dg_cast 'cure light'
~
#16459
Verdant Plains - Thornbound Venom~
0 k 12
~
eval thornroll %random.100%
if %thornroll% < 50
  dg_cast 'poison' %actor%
else
  dg_cast 'web' %actor%
end
~
#16460
Verdant Plains - Mossback Stone Skin~
0 k 12
~
dg_cast 'stone skin'
~
#16461
Verdant Plains - Druid Barkskin~
0 k 12
~
dg_cast 'barkskin'
~
#16462
Verdant Plains - Skywarden Stormcall~
0 k 12
~
dg_cast 'call lightning' %actor%
~
#16463
Verdant Plains - Stonekeeper Ward~
0 k 12
~
dg_cast 'stone skin'
~
#16464
Verdant Plains - Elder Bloom Cycle~
0 k 20
~
eval bloomroll %random.100%
if %bloomroll% < 34
  dg_cast 'web' %actor%
elseif %bloomroll% < 67
  dg_cast 'barkskin'
else
  dg_cast 'cure light'
end
~
$~
