from pathlib import Path
root = Path(__file__).resolve().parents[1]
read = lambda p: (root / p).read_text()
h = read('src/ai_actor.h'); lb = read('src/legacy_behavior.c'); db = read('src/db.c'); gen = read('src/genmob.c'); medit = read('src/medit.c'); oasis = read('src/oasis.h'); mobact = read('src/mobact.c')
assert 'behavior_owner[13]' in h
assert 'AIBehaviorOwner' in db and 'AIBehaviorOwner' in gen
assert 'mob_behavior_domain_from_token' in lb and 'mob_behavior_owner_from_token' in lb
assert 'MEDIT_AI_OWNERSHIP' in oasis and 'MEDIT_AI_OWNERSHIP_VALUE' in oasis and 'MEDIT_AI_OWNERSHIP_RESET' in oasis
for mode in ['MEDIT_AI_OWNERSHIP:', 'MEDIT_AI_OWNERSHIP_VALUE:', 'MEDIT_AI_OWNERSHIP_RESET:']:
    assert mode in medit
assert 'medit_parse_ai_integer(arg,1,4,&i)' in medit
assert "LOWER(*arg)=='q'" in medit
assert 'AI ownership unavailable: legacy Mayor special must be migrated first' in medit
assert 'Ownership locked by unknown custom special' in medit
assert 'DG Script behavior is not controlled by Phase 2B ownership' in medit
assert 'Service commands are outside Phase 2B arbitration' in medit
assert 'ai_actor_tick_with_context' in mobact
for gate in ['LBD_MOVEMENT','LBD_SCAVENGING','LBD_COMBAT_INIT','LBD_MEMORY','LBD_HELPER']:
    assert f'mob_behavior_domain_available_to_legacy_tail(&ctx,{gate})'.replace(' ', '') in mobact.replace(' ', '')
assert 'mobile_activity_legacy_preserving();' in mobact
print('phase2b structural checks passed')
