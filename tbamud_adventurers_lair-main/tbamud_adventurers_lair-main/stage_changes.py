from pathlib import Path
ROOT=Path(r'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED')
OUT=Path('audit_stage')
def read(f):return (ROOT/f).read_bytes().decode('utf-8')
def save(f,s):
    p=OUT/f;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(s.encode('utf-8'))
profiles=[('INSECTOID','Insectoid'),('WINGED_INSECTOID','Winged Insectoid'),('CONSTRUCT','Construct'),('AMORPHOUS','Amorphous'),('FISH','Fish'),('PLANT','Plant'),('CENTAUROID','Centauroid'),('CRUSTACEAN','Crustacean'),('WINGED_QUADRUPED','Winged Quadruped'),('TENTACLED','Tentacled'),('BAT','Bat')]
parts=[('MIDLEG_LEFT','severed left middle leg'),('MIDLEG_RIGHT','severed right middle leg'),('REARLEG_LEFT','severed left rear leg'),('REARLEG_RIGHT','severed right rear leg'),('FIN_LEFT','torn left fin'),('FIN_RIGHT','torn right fin'),('BRANCH','broken branch'),('ROOT','torn root'),('CLAW_LEFT','severed left claw'),('CLAW_RIGHT','severed right claw')]
s=read('src/structs.h');nl='\r\n' if '\r\n' in s else '\n'
s=s.replace('#define NUM_BODY_PROFILES      9',nl.join(f'#define BODY_PROFILE_{key} {i}' for i,(key,_) in enumerate(profiles,9))+nl+'#define NUM_BODY_PROFILES      20')
anchor='#define BODY_PART_TENTACLE_2    (1 << 15)'
s=s.replace(anchor,anchor+nl+nl.join(f'#define BODY_PART_{key} (1 << {i})' for i,(key,_) in enumerate(parts,16))+nl+'#define BODY_PART_VALID_MASK ((1 << 26) - 1)')
save('src/structs.h',s)
s=read('src/fight.c');nl='\r\n' if '\r\n' in s else '\n'
s=s.replace('  default: return "None";',nl.join(f'  case BODY_PROFILE_{key}: return "{name}";' for key,name in profiles)+nl+'  default: return "None";',1)
s=s.replace('static int body_profile_parts(int profile)','int body_profile_parts(int profile)',1)
s=s.replace('BODY_PART_HINDLEG_RIGHT | BODY_PART_TENTACLE_1 | BODY_PART_TENTACLE_2;', 'BODY_PART_HINDLEG_RIGHT | BODY_PART_MIDLEG_LEFT | BODY_PART_MIDLEG_RIGHT |'+nl+'      BODY_PART_REARLEG_LEFT | BODY_PART_REARLEG_RIGHT;',1)
extra='''  case BODY_PROFILE_INSECTOID:
    return BODY_PART_HEAD | BODY_PART_TORSO | BODY_PART_FORELEG_LEFT |
      BODY_PART_FORELEG_RIGHT | BODY_PART_MIDLEG_LEFT | BODY_PART_MIDLEG_RIGHT |
      BODY_PART_HINDLEG_LEFT | BODY_PART_HINDLEG_RIGHT;
  case BODY_PROFILE_WINGED_INSECTOID:
    return body_profile_parts(BODY_PROFILE_INSECTOID) | BODY_PART_WING_LEFT | BODY_PART_WING_RIGHT;
  case BODY_PROFILE_CONSTRUCT:
  case BODY_PROFILE_AMORPHOUS:
    /* No standardized organic trophies; the ordinary loot-bearing corpse remains. */
    return 0;
  case BODY_PROFILE_FISH:
    return BODY_PART_HEAD | BODY_PART_TORSO | BODY_PART_TAIL | BODY_PART_FIN_LEFT | BODY_PART_FIN_RIGHT;
  case BODY_PROFILE_PLANT:
    return BODY_PART_BRANCH | BODY_PART_ROOT;
  case BODY_PROFILE_CENTAUROID:
    return body_profile_parts(BODY_PROFILE_QUADRUPED) | BODY_PART_ARM_LEFT | BODY_PART_ARM_RIGHT;
  case BODY_PROFILE_CRUSTACEAN:
    return body_profile_parts(BODY_PROFILE_ARACHNID) | BODY_PART_CLAW_LEFT | BODY_PART_CLAW_RIGHT;
  case BODY_PROFILE_WINGED_QUADRUPED:
    return body_profile_parts(BODY_PROFILE_QUADRUPED) | BODY_PART_WING_LEFT | BODY_PART_WING_RIGHT;
  case BODY_PROFILE_TENTACLED:
    /* Harvestable representative tentacles, not a complete limb census. */
    return BODY_PART_HEAD | BODY_PART_TORSO | BODY_PART_TENTACLE_1 | BODY_PART_TENTACLE_2;
  case BODY_PROFILE_BAT:
    return body_profile_parts(BODY_PROFILE_AVIAN) | BODY_PART_TAIL;
'''.replace('\n',nl)
anchor='  default:'+nl+'    return 0;'+nl+'  }'+nl+'}'+nl+nl+'int resolve_body_profile'
assert anchor in s;s=s.replace(anchor,extra+anchor,1)
s=s.replace('  { BODY_PART_TENTACLE_2,     7 }','  { BODY_PART_TENTACLE_2,     7 },'+nl+(','+nl).join(f'  {{ BODY_PART_{key}, 7 }}' for key,_ in parts),1)
s=s.replace('  default: return "mutilated remains";',nl.join(f'  case BODY_PART_{key}: return "{name}";' for key,name in parts)+nl+'  default: return "mutilated remains";',1)
save('src/fight.c',s)
s=read('src/fight.h').replace('const char *body_profile_name(int profile);','int body_profile_parts(int profile);\nconst char *body_profile_name(int profile);');save('src/fight.h',s)
s=read('src/act.offensive.c');start=s.index('  int valid_part_mask =');end=s.index(';',start)+1
s=s[:start]+'  int valid_part_mask = BODY_PART_VALID_MASK;'+s[end:]
s=s.replace('if (REMAINS_PROFILE(corpse) == BODY_PROFILE_NONE)','if (!(body_profile_parts(REMAINS_PROFILE(corpse)) & BODY_PART_HEAD))')
save('src/act.offensive.c',s)
s=read('src/medit.c');start=s.index('      write_to_output(d,\n        "Body profile:') if '\r\n' not in s else s.index('      write_to_output(d,\r\n        "Body profile:');end=s.index('      return;',start)
nl='\r\n' if '\r\n' in s else '\n'
s=s[:start]+('''      write_to_output(d, "Body profile (-1 = Automatic):\\r\\n");
      for (i = BODY_PROFILE_NONE; i < NUM_BODY_PROFILES; i++)
        write_to_output(d, "%2d) %s\\r\\n", i, body_profile_name(i));
      write_to_output(d, "Enter profile number: ");
'''.replace('\n',nl))+s[end:]
s=s.replace('if (i < BODY_PROFILE_NONE || i >= NUM_BODY_PROFILES)', 'if (!is_number(arg) || i < -1 || i >= NUM_BODY_PROFILES)')
s=s.replace('"Choose a profile number from 0 to %d: "','"Choose -1 for Automatic or a profile number from 0 to %d: "')
s=s.replace('GET_MOB_BODY_PROFILE(OLC_MOB(d)) = i;', 'GET_MOB_BODY_PROFILE(OLC_MOB(d)) = i < 0 ? BODY_PROFILE_NONE : i;')
s=s.replace('MOB_BODY_PROFILE_SET(OLC_MOB(d)) = 1;', 'MOB_BODY_PROFILE_SET(OLC_MOB(d)) = (i >= 0);')
save('src/medit.c',s)
# Script affect is registered for DG persistence/display, not player learning.
s=read('src/tome.c').replace('return ability > 0 && ability <= TOP_SPELL_DEFINE && spell_info[ability].name &&','return ability > 0 && ability <= TOP_SPELL_DEFINE && ability <= MAX_SKILLS &&\n      ability != SPELL_DG_AFFECT && spell_info[ability].name && *spell_info[ability].name &&')
save('src/tome.c',s)
s=read('src/act.other.c').replace('return (id > 0 &&','return (id != SPELL_DG_AFFECT && id > 0 &&',1)
save('src/act.other.c',s)
# Hostile blindness must use the same peaceful-room/reflection checks as curse.
s=read('src/spell_parser.c').replace('TAR_CHAR_ROOM | TAR_NOT_SELF, FALSE, MAG_AFFECTS,','TAR_CHAR_ROOM | TAR_NOT_SELF, TRUE, MAG_AFFECTS,',1)
save('src/spell_parser.c',s)
print('Staged',len(list(OUT.rglob('*.*'))),'files')
