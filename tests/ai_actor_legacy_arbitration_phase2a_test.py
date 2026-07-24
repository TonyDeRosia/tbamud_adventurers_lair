#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def read(p): return (ROOT / p).read_text()

def require(cond, msg):
    if not cond: raise AssertionError(msg)

h = read('src/legacy_behavior.h')
c = read('src/legacy_behavior.c')
mobact = read('src/mobact.c')
aih = read('src/ai_actor.h')
aic = read('src/ai_actor.c')
medit = read('src/medit.c')
doc = read('docs/AI_ACTOR_LEGACY_INTEGRATION_PHASE2A.md')
actw = read('src/act.wizard.c')

for token in ['MobBehaviorDomainMask','mob_behavior_pulse_context','mob_behavior_action_result','mob_behavior_owner','MOB_BEHAVIOR_COMPAT_LEGACY_PRESERVING']:
    require(token in h, f'missing arbitration type {token}')
for dom in ['LBD_ROUTINE','LBD_MOVEMENT','LBD_POSTURE','LBD_AMBIENT_SPEECH','LBD_COMBAT_INIT','LBD_MEMORY','LBD_HELPER','LBD_SCAVENGING','LBD_FLEE']:
    require(dom in h and dom in c, f'missing phase2a domain {dom}')
for owner in ['COMPATIBILITY','LEGACY','AI','DISABLED']:
    require(f'MOB_BEHAVIOR_OWNER_{owner}' in h, f'missing owner {owner}')

require('CONFIG_AI_LEGACY_ARBITRATION_ENABLED' in mobact, 'mobile_activity lacks feature flag')
require('mobile_activity_legacy_preserving();' in mobact, 'rollback seam missing')
require('ai_actor_tick_with_context' in aih and 'ai_actor_tick_with_context' in aic, 'context AI tick missing')
for gate in ['LBD_MOVEMENT','LBD_ROUTINE','LBD_POSTURE','LBD_AMBIENT_SPEECH','LBD_COMBAT_INIT']:
    require(f'mob_behavior_domain_available_to_ai(ctx, {gate})' in aic, f'AI gate missing {gate}')
require('mob_behavior_domain_available_to_legacy_tail' in c, 'legacy tail gate helper missing')
require('AI ownership unavailable: legacy Mayor special must be migrated first.' in c + medit + doc, 'Mayor lock message missing')
require('Unknown custom special: conservative legacy lock' in c, 'unknown-special lock missing')
require('Behavior Ownership' in medit and 'Q) Return' in medit, 'MEDIT ownership screen/cancel missing')
require('Service Commands:\\r\\n  Legacy service, outside Phase 2A' in medit, 'service outside notice missing')
require('DG Scripts:\\r\\n  External behavior authority; not arbitrated in Phase 2A' in medit, 'DG outside notice missing')
require('mob_behavior_recent_pulse_report' in actw, 'admin live pulse diagnostic missing')
require('No persistence contract is added in Phase 2A' in doc, 'persistence decision undocumented')
require('SENTINEL and STAY_ZONE remain movement restrictions' in doc, 'movement restrictions undocumented')
require('perform_violence' in doc or 'combat tactics' in doc, 'outside combat scope undocumented')
print('Phase 2A arbitration source checks passed')
