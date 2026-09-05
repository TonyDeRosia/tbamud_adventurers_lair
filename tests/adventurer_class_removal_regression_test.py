#!/usr/bin/env python3
"""Execute production class tables/parsers and verify retired-class integration."""
from pathlib import Path
import hashlib
import os
import re
import shlex
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
def src(name):
    return (ROOT / 'src' / name).read_text(encoding='utf-8')

def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

CLASSES = ['MAGIC_USER', 'CLERIC', 'THIEF', 'WARRIOR', 'PALADIN', 'BARD', 'WARLOCK', 'DRUID', 'MYSTIC']
CLASS = src('class.c')

def run_c(body):
    include = str(ROOT / 'src')
    prefix = []
    if os.name == 'nt':
        include = '/mnt/' + include[0].lower() + include[2:].replace('\\', '/')
        prefix = ['wsl', '--exec']
    script = ('out=$(mktemp /tmp/class-retirement.XXXXXX); trap \'rm -f "$out"\' EXIT; '
              'gcc -std=gnu17 -fsanitize=undefined -fno-sanitize-recover=all -I ' + shlex.quote(include) +
              ' -x c - -o "$out" && "$out"')
    result = subprocess.run(prefix + ['bash', '-lc', script], input=body, text=True, capture_output=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout

HEADERS = '''
#include "conf.h"
#include "sysdep.h"
#include "structs.h"
#include "utils.h"
#include "class.h"
#include "spells.h"
#include "db.h"
#include <assert.h>
#undef log
#define log(...) ((void)0)
#define mudlog(...) ((void)0)
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#define SPELL 0
#define SKILL 1
'''

def class_helpers():
    return (CLASS[CLASS.index('const struct pc_class_definition pc_classes[]'):CLASS.index('const char *get_archetype_abbrev')]
            + '\n' + function(CLASS, 'const char *class_menu')
            + '\n' + function(CLASS, 'int parse_class')
            + '\n' + function(CLASS, 'static int get_class_archetype'))

class AdventurerRemoval(unittest.TestCase):
    def test_persistent_ids_and_no_retired_gameplay_symbol(self):
        structs = src('structs.h')
        for i, name in enumerate(CLASSES):
            self.assertRegex(structs, rf'#define CLASS_{name}\s+{i}\b')
        self.assertRegex(structs, r'#define NUM_CLASSES\s+9\b')
        self.assertRegex(structs, r'#define RETIRED_PC_CLASS_ID\s+9\b')
        for path in (ROOT/'src').rglob('*'):
            if path.suffix in ('.c', '.h'):
                self.assertNotIn('CLASS_ADVENTURER', path.read_text(encoding='utf-8'), str(path))

    def test_production_menu_parser_tables_and_archetypes(self):
        body = HEADERS + class_helpers() + r'''
int main(void) {
  const char *names[]={"Mage","Cleric","Thief","Warrior","Paladin","Bard","Warlock","Druid","Mystic"};
  const char *abbr[]={"Mg","Cl","Th","Wa","Pa","Br","Wl","Dr","My"};
  const char *keys="MCTWPBKDY";
  int archetypes[]={0,1,2,3,3,2,0,1,0};
  int practice[]={95,95,85,80,80,85,95,95,95};
  assert(num_pc_classes()==9);
  for(int i=0;i<9;i++) {
    assert(is_valid_class(i)); assert(parse_class(keys[i])==i);
    assert(parse_class(tolower(keys[i]))==i);
    assert(!strcmp(class_name(i),names[i])); assert(!strcmp(class_abbrev(i),abbr[i]));
    assert(pc_classes[i].selectable); assert(pc_classes[i].prac_learned_level==practice[i]);
    assert(get_class_archetype(i)==archetypes[i]); assert(strstr(class_menu(),names[i]));
  }
  assert(!strstr(class_menu(),"Adventurer"));
  assert(parse_class('a')==CLASS_UNDEFINED); assert(parse_class('9')==CLASS_UNDEFINED);
  assert(parse_class('\0')==CLASS_UNDEFINED);
  for(int id=-128;id<128;id++) if(id<0 || id>8) {
    assert(!is_valid_class(id)); assert(!strcmp(class_name(id),"Unknown"));
    assert(!strcmp(class_abbrev(id),"Unknown"));
  }
  puts("Nine production classes, keys, names, practice settings and archetypes passed.");
}
'''
        self.assertIn('passed', run_c(body))

    def test_resource_gains_preserved_for_all_nine_classes(self):
        block = function(CLASS, 'void advance_level')
        expected = [(18,18,8),(23,15,9),(28,7,14),(34,5,11),(29,12,10),
                    (22,13,13),(20,18,9),(24,16,11),(26,14,12)]
        actual = re.findall(r'\[CLASS_(\w+)\]\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}', block)
        self.assertEqual([(name,tuple(map(int,(h,m,v)))) for name,h,m,v in actual],list(zip(CLASSES,expected)))
        self.assertLess(block.index('!is_valid_class(GET_CLASS(ch))'), block.index('gain = &gains[GET_CLASS(ch)]'))

    def test_catalogs_always_use_real_class(self):
        act = src('act.other.c')
        self.assertNotIn('show_adventurer_study_catalog', act)
        self.assertNotIn('As an Adventurer', act)
        for command, kind in [('do_skills',0),('do_spells',1)]:
            body = function(act, f'ACMD({command})')
            self.assertIn(f'show_ability_table_aligned(ch, {kind}, show_all, filter)', body)
            self.assertNotIn('GET_CLASS_LOCKED', body)
        formatter = function(act, 'void show_ability_table_aligned')
        self.assertIn('int cls = GET_CLASS(ch);', formatter)
        self.assertLess(formatter.index('!is_valid_class(cls)'), formatter.index('min_level[cls]'))
        self.assertIn('has_tome_ability(ch, i)', formatter)

    def test_production_legacy_class_tag_rejected_before_narrowing(self):
        players = src('players.c')
        branch = function(players, 'else if (!strcmp(tag, "Clas"))')
        branch = branch[branch.index('{'):]
        body = HEADERS + class_helpers() + r'''
static int closed;
#define fclose(f) (++closed)
int parse_saved_class(const char *line) {
  struct char_data storage = {0}, *ch=&storage;
  FILE *fl=NULL;
  const char *filename="fixture.plr";
''' + branch + r'''
  return GET_CLASS(ch);
}
int main(void) {
  for(int i=0;i<9;i++) { char text[16]; snprintf(text,sizeof(text),"%d",i); assert(parse_saved_class(text)==i); }
  const char *bad[]={"9","10","127","256","-1","","   ","garbage","3junk","9999999999999999999999999"};
  for(size_t i=0;i<ARRAY_SIZE(bad);i++) assert(parse_saved_class(bad[i])==LOAD_CHAR_INVALID_CLASS);
  assert(closed==ARRAY_SIZE(bad));
  assert(parse_saved_class(" 4 ")==4);
  puts("Legacy/invalid class rejection passed without narrowing or migration.");
}
'''
        self.assertIn('passed', run_c(body))
        self.assertLess(players.index('return LOAD_CHAR_INVALID_CLASS'),players.index('clamp_player_exp_to_level(ch);'))
        self.assertLess(players.index('return LOAD_CHAR_INVALID_CLASS'),players.index('ensure_class_abilities(ch);'))
        nanny=src('interpreter.c')
        failure=nanny.index('if (player_i == LOAD_CHAR_INVALID_CLASS)')
        creation=nanny.index('/* player unknown -- make new character */')
        self.assertIn('return;',nanny[failure:creation])
        self.assertIn('CON_ACCT_MENU',nanny[failure:creation])
        self.assertIn('LOAD_CHAR_INVALID_CLASS',src('comm.c'))
        self.assertIn('!is_valid_class(GET_CLASS(ch))',function(players,'int save_char'))
        self.assertIn('byte chclass;',src('util/plrtoascii.c'))
        self.assertIn('player.chclass',src('util/plrtoascii.c'))

    def test_set_class_and_titles_preserved(self):
        wizard=src('act.wizard.c')
        self.assertIn('parse_class(*val_arg)',wizard)
        self.assertIn('GET_CLASS(vict) = i;',wizard)
        self.assertIn('!pc_classes[chclass].selectable',src('interpreter.c'))
        self.assertIn('C, R, GET_TITLE(ch)',src('act.informative.c'))
        self.assertIn('"the %s", GET_SOFT_CLASS_TITLE(ch)',src('limits.c'))
        ct=src('classtrack.c')
        self.assertIn('classtrack_set_title(ch, "Adventurer");',ct)
        self.assertIn('GET_CLASS_LOCKED(ch) = 1;',ct)
        self.assertIn('GET_ARCHETYPE_SCORE(ch, archetype) += gain;',ct)
        self.assertIn('"ClLo"',src('players.c'))

    def test_production_ability_assignments_all_classes(self):
        body=HEADERS+class_helpers()+'''\nstruct spell_info_type spell_info[TOP_SPELL_DEFINE+1];
const char *skill_name(int skill) { return "test"; }
'''+function(src('spell_parser.c'),'void spell_level(')+'\n'+function(CLASS,'void init_spell_levels')+r'''
int main(void) {
  for(int a=0;a<=TOP_SPELL_DEFINE;a++) for(int c=0;c<MAX_CLASSES;c++) spell_info[a].min_level[c]=LVL_IMMORT;
  init_spell_levels();
  for(int c=0;c<9;c++) {
    assert(spell_info[SKILL_STUDY].min_level[c]==LVL_IMMORT);
    assert(spell_info[SKILL_RECALL].min_level[c]==1);
    assert(spell_info[SKILL_UNARMED].min_level[c]==1);
    assert(spell_info[SKILL_KICK].min_level[c]==5);
  }
  spell_level(SKILL_STUDY,9,1);
  assert(spell_info[SKILL_STUDY].min_level[9]==LVL_IMMORT);
  for(int c=0;c<9;c++) for(int a=1;a<=TOP_SPELL_DEFINE;a++)
    printf("%d:%d:%d\n",c,a,spell_info[a].min_level[c]);
}
'''
        output=run_c(body)
        # Golden full class/ability table from the pre-removal working tree (IDs 0-8).
        self.assertEqual(hashlib.sha256(output.encode()).hexdigest(), '3ad14b3bb83a3a29f23b698abddb82a86c2563c934be1d055af201ecb4211327')

if __name__=='__main__':
    unittest.main(verbosity=2)
