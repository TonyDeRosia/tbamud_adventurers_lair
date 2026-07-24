#!/usr/bin/env python3
"""Structural guardrails for Oasis AI ownership routing and its shared model."""
import re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
oasis=(root/'src/oasis.h').read_text(); medit=(root/'src/medit.c').read_text()
lbh=(root/'src/legacy_behavior.h').read_text(); lbc=(root/'src/legacy_behavior.c').read_text(); aih=(root/'src/ai_actor.h').read_text()
defines=[(n,int(v)) for n,v in re.findall(r'^#define\s+(MEDIT_[A-Z0-9_]+)\s+(\d+)\s*$',oasis,re.M)]
values={}
for name,value in defines:
 assert value not in values, f'duplicate MEDIT value {value}: {values.get(value)}, {name}'
 values[value]=name
ai=[n for n,_ in defines if n.startswith('MEDIT_AI_')]
helper=re.search(r'static int medit_is_ai_mode\(int mode\)\s*\{(.*?)\n\}',medit,re.S).group(1)
for name in ai: assert f'case {name}:' in helper, f'{name} not explicitly routed'
parser=medit[medit.index('void medit_parse('):]
for name in ('MEDIT_AI_OWNERSHIP','MEDIT_AI_OWNERSHIP_VALUE','MEDIT_AI_OWNERSHIP_RESET'):
 assert f'case {name}:' in parser
 assert name in helper
assert 'mode >=' not in helper and 'mode <=' not in helper
count=int(re.search(r'#define MOB_BEHAVIOR_DOMAIN_COUNT\s+(\d+)',lbh).group(1))
assert count==13
assert 'behavior_owner[MOB_BEHAVIOR_DOMAIN_COUNT]' in aih
assert len(re.findall(r'\{LBD_[A-Z_]+,"[^\"]+","[^\"]+",(?:TRUE|FALSE)\}',lbc))==count
assert '_Static_assert(sizeof(behavior_domains)' in lbc
assert 'mob_behavior_domain_token(domain)' in (root/'src/genmob.c').read_text()
print(f'PASS: {len(defines)} unique MEDIT modes; {len(ai)} AI modes explicitly routed; {count} domains aligned')
