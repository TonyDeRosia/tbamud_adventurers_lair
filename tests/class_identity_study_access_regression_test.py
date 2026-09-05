#!/usr/bin/env python3
"""Source contracts plus execution of production Tome/access helpers in a C harness.

Run with py -3 on Windows (WSL GCC) or python3 on Linux (GCC).
The harness substitutes only engine storage/I/O, not acquisition decisions.
"""
from pathlib import Path
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(name):
    return (ROOT / 'src' / name).read_text(encoding='utf-8')

ACT, CT, CLASS, TOME, PRACTICE = map(source,
    ['act.other.c', 'classtrack.c', 'class.c', 'tome.c', 'spec_procs.c'])

def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

class StudyRegression(unittest.TestCase):
    def test_catalog_identity_and_lock_persistence(self):
        for name, spell in [('do_skills', 0), ('do_spells', 1)]:
            body = function(ACT, f'ACMD({name})')
            self.assertNotIn('GET_CLASS_LOCKED', body)
            self.assertNotIn('show_adventurer_study_catalog', body)
            self.assertIn(f'show_ability_table_aligned(ch, {spell}, show_all, filter)', body)
        self.assertIn('GET_CLASS_LOCKED(ch) = 1;', CT)
        self.assertIn('GET_CLASS_LOCKED(ch) = 0;', CT)
        self.assertIn('"ClLo"', source('players.c'))
        self.assertIn('return class_name(GET_CLASS(ch));', CT)

    def test_study_registration_without_class_assignment(self):
        self.assertIn('#define SKILL_STUDY                 260', source('spells.h'))
        parser = source('spell_parser.c')
        self.assertIn('skillo_cost(SKILL_STUDY, "study", 0);', parser)
        self.assertIn('spell_info[spl].min_level[i] = LVL_IMMORT;', parser)
        self.assertNotRegex(CLASS, r'spell_level\s*\(\s*SKILL_STUDY')
        self.assertNotIn('classtrack_ensure_starter_learn_levels', CT)
        for name in ['class.c', 'players.c', 'db.c']:
            self.assertNotIn('classtrack_ensure_starter_learn_levels(ch);', source(name))
            self.assertNotRegex(source(name), r'SET_SKILL\s*\(ch,\s*SKILL_STUDY')
        self.assertIn('spell_level(SKILL_RECALL, i, 1);', CLASS)
        self.assertIn('spell_level(SKILL_UNARMED, i, 1);', CLASS)

    def test_legacy_listing_and_shared_tome_display(self):
        body = function(ACT, 'void show_ability_table_aligned')
        self.assertIn('i == SKILL_STUDY && !character_has_ability_access(ch, i)', body)
        self.assertIn('has_tome_ability(ch, i)', body)
        self.assertNotIn('lvl = 1;', body)
        self.assertNotIn('SKILL_STUDY', function(CT, 'int classtrack_get_study_display_level'))

    def test_tome_routing_before_authorization_and_side_effects(self):
        body = function(ACT, 'ACMD(do_study)')
        route = body.index('tome_study_object(ch, tome_obj);')
        gate = body.index('!character_has_ability_access(ch, SKILL_STUDY)')
        self.assertLess(route, gate)
        self.assertIn('return;', body[route:gate])
        self.assertIn('GET_LEVEL(ch) < LVL_IMMORT', body[route:gate])
        end = body.index('if (study_is_on_cooldown', gate)
        self.assertIn('GET_SKILL(ch, SKILL_STUDY) <= 0', body[gate:end])
        self.assertIn('return;', body[gate:end])
        self.assertNotIn('study_apply_cooldown', body[:end])
        self.assertNotIn('extract_obj', body[:end])
        self.assertNotIn('SET_SKILL', body[:end])
        self.assertLess(gate, body.index('study_attempt_succeeds'))

    def test_study_neutral_archetype(self):
        self.assertNotRegex(CT, r'\{\s*SKILL_STUDY\s*,\s*ARCHETYPE_')
        self.assertIn('if (ability_id == SKILL_STUDY)\n    return 0;', CT)

    def test_titles_and_persistence_preserved(self):
        self.assertIn('C, R, GET_TITLE(ch)', source('act.informative.c'))
        self.assertIn('the %s', source('limits.c'))
        for marker in ['TmAb:', 'TmCd:']:
            self.assertIn(marker, source('players.c'))

    def test_production_tome_and_practice_behavior(self):
        prelude = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdarg.h>
#define TRUE 1
#define FALSE 0
#define LVL_IMMORT 101
#define MAX_SKILLS 300
#define TOP_SPELL_DEFINE 300
#define MAX_SPELLS 200
#define SKILL_STUDY 260
#define SPELL_MAGIC_MISSILE 1
#define SKILL_RECALL 201
#define NUM_PC_CLASSES 9
#define TOME_DEFAULT_OFFCLASS_AFFINITY 50
#define ITEM_TOME 24
#define TOME_ABILITY_SLOTS 4
#define MAX_INPUT_LENGTH 1024
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
struct char_data { int npc, level, cls, skills[301], markers[301], learned[301]; time_t expires; };
struct obj_data { int type, val[4], tome_cooldown_seconds; char *short_description; };
struct { const char *name; int min_level[9]; } spell_info[301];
const char *unused_spellname = "!UNUSED!";
#define IS_NPC(c) ((c)->npc)
#define GET_LEVEL(c) ((c)->level)
#define GET_CLASS(c) ((c)->cls)
#define GET_SKILL(c,a) ((c)->skills[a])
#define SET_SKILL(c,a,v) ((c)->skills[a]=(v))
#define HAS_TOME_ABILITY(c,a) ((c)->markers[a])
#define SET_TOME_ABILITY(c,a) ((c)->markers[a]=1)
#define GET_STUDY_LEARN_LEVEL(c,a) ((c)->learned[a])
#define SET_STUDY_LEARN_LEVEL(c,a,v) ((c)->learned[a]=(v))
#define GET_TOME_STUDY_EXPIRES_AT(c) ((c)->expires)
#define GET_OBJ_TYPE(o) ((o)->type)
#define GET_OBJ_VAL(o,i) ((o)->val[i])
#define str_cmp strcasecmp
static int consumed, saved;
static char output[8192];
void send_to_char(struct char_data *ch, const char *fmt, ...) {
  va_list args; va_start(args,fmt);
  vsnprintf(output+strlen(output), sizeof(output)-strlen(output),fmt,args);
  va_end(args);
}
void extract_obj(struct obj_data *o) { consumed++; }
void save_char(struct char_data *c) { saved++; }
'''
        production = function(CLASS, 'int is_valid_class') + '\n' + TOME[TOME.index('int tome_valid_ability'):TOME.index('ACMD(do_tome)')]
        production += '\n' + function(CT, 'int classtrack_get_study_display_level')
        production += '\n' + function(PRACTICE, 'static int can_character_practice_ability')
        main = r'''
int main(void) {
  for(int a=0;a<=300;a++) for(int c=0;c<9;c++) spell_info[a].min_level[c]=LVL_IMMORT;
  spell_info[260].name="study"; spell_info[1].name="magic missile";
  spell_info[2].name="other"; spell_info[3].name="native";
  for(int cls=0;cls<9;cls++) {
    struct char_data fresh={.level=1,.cls=cls};
    assert(!GET_SKILL(&fresh,260));
    assert(!character_has_ability_access(&fresh,260));
    assert(!can_character_practice_ability(&fresh,260));
    for(int pct=0;pct<=75;pct+=25) {
      struct char_data c={.level=1,.cls=cls}; c.skills[260]=pct; c.learned[260]=1;
      assert(!can_character_practice_ability(&c,260));
      struct obj_data o={.type=ITEM_TOME,.val={260},.tome_cooldown_seconds=90,.short_description="a tome"};
      int before=consumed; time_t now=time(NULL); output[0]=0;
      assert(tome_valid_ability(260)); assert(tome_study_object(&c,&o));
      assert(c.markers[260] && c.skills[260]==(pct?pct:1));
      assert(consumed==before+1 && c.expires>=now+90);
      assert(character_has_ability_access(&c,260));
      assert(can_character_practice_ability(&c,260));
      assert(get_ability_class_affinity(&c,260)==50);
      assert(classtrack_get_study_display_level(&c,260,1)==1);
      c.expires=0; before=consumed;
      assert(!tome_study_object(&c,&o));
      assert(consumed==before && c.expires==0);
    }
  }
  struct char_data c={.level=1,.cls=0};
  struct obj_data o={.type=ITEM_TOME,.val={3},.tome_cooldown_seconds=60,.short_description="a tome"};
  spell_info[3].min_level[0]=1; c.skills[3]=55;
  int before=consumed;
  assert(!tome_study_object(&c,&o)); assert(!c.markers[3]);
  assert(consumed==before && c.expires==0);
  o.val[0]=2; c.skills[2]=80;
  assert(tome_study_object(&c,&o)); assert(c.skills[2]==80 && c.markers[2]);
  assert(strstr(output,"at 80%"));
  o.val[0]=1; before=consumed;
  assert(!tome_study_object(&c,&o)); assert(!c.markers[1] && consumed==before);
  c.expires=0; o.val[1]=1;
  assert(!tome_study_object(&c,&o)); assert(consumed==before);
  o.val[1]=0; o.val[0]=299;
  assert(!tome_study_object(&c,&o)); assert(consumed==before);
  c.level=LVL_IMMORT; assert(character_has_ability_access(&c,260));
  assert(can_character_practice_ability(&c,260));
  assert(saved==consumed);
  for (int invalid=-1; invalid<=127; invalid++) {
    if (invalid>=0 && invalid<9) continue;
    c.cls=invalid; c.markers[260]=1; c.skills[260]=75;
    assert(!character_has_ability_access(&c,260));
    assert(!can_character_practice_ability(&c,260));
    assert(get_ability_class_affinity(&c,260)==0);
    assert(!tome_study_object(&c,&o));
  }
  puts("Production C harness passed: 9 classes, 36 Study Tome acquisitions, legacy/native/repeat/cooldown/validation/practice cases");
}
'''
        command = ['bash', '-lc', 'out=$(mktemp /tmp/study-regression.XXXXXX); trap \'rm -f "$out"\' EXIT; gcc -std=gnu17 -fsanitize=undefined -fno-sanitize-recover=all -x c - -o "$out" && "$out"']
        if os.name == 'nt':
            command[:0] = ['wsl', '--exec']
        result = subprocess.run(command, input=prelude+production+main, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        print(result.stdout.strip())

if __name__ == '__main__':
    unittest.main(verbosity=2)
