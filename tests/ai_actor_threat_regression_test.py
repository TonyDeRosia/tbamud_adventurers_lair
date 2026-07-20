#!/usr/bin/env python3
"""Deterministic source-level regression guard for threat policy helpers."""
from pathlib import Path
s = Path('src/ai_actor.c').read_text()
h = Path('src/ai_actor.h').read_text()
assert 'AI_DIALOGUE_WARNING' in h and 'AI_DIALOGUE_FEAR' in h
assert 'calm_reset_time' in h
assert 'ai_threat_severity' in s and 'AI_CLAMP(v*AI_CLAMP(confidence,0,100)/100,0,100)' in s
assert 'ai_threat_response_targeted' in s
assert 'm->identity_confidence < mob->ai_prof->recognition_confidence' in s
assert 'AI_DIALOGUE_WARNING' in s and 'AI_DIALOGUE_CHALLENGE' in s and 'AI_DIALOGUE_THREAT' in s
assert 'AI_DIALOGUE_CALL_HELP' in s and 'AI_DIALOGUE_FEAR' in s
assert 'm->threat_help_called' in s
assert 'hit(mob,actor,0)' in s
assert 'ai_threat_step_valid' in s and 's->cooldown == 0 && s->max_repetitions > 1' in s
print('ai actor threat regression checks passed')
