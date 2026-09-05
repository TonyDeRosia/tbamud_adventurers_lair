import re,json
from pathlib import Path
from collections import Counter
ROOT=Path(r'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED')
src={p.name:p.read_text(encoding='utf-8',errors='replace') for p in (ROOT/'src').glob('*') if p.suffix in ('.h','.c')}
defs={m[1]:int(m[2]) for m in re.finditer(r'^#define\s+((?:SPELL_|SKILL_|MAX_SPELLS|TOP_SPELL_DEFINE)\w*)\s+(\d+)\b',src['spells.h'],re.M)}
def splitargs(s):
    return [x.strip() for x in re.split(r',(?=(?:[^"\\]*(?:\\.[^"\\]*)*"[^"\\]*(?:\\.[^"\\]*)*")*[^"\\]*$)',s)]
reg=src['spell_parser.c'].split('void mag_assign_spells(void)')[-1]
rows=[]
for m in re.finditer(r'\b(spello|skillo_cost|skillo)\((.*?)\);',reg,re.S):
    a=splitargs(m[2]); symbol=a[0]
    if symbol not in defs: continue
    if m[1]=='spello':
        if len(a)!=10: raise ValueError(a)
        _,name,maxcost,mincost,change,pos,target,violent,mechanism,msg=a
    else:
        name=a[1]; maxcost=mincost=a[2] if len(a)>2 else '10';change=pos=target=violent=mechanism='0';msg='NULL'
    ref=[]; funcs=[];cool=[];improve=[]
    for file,text in src.items():
        if not file.endswith('.c'):continue
        for hit in re.finditer(r'\b'+symbol+r'\b',text):
            ln=text.count('\n',0,hit.start())+1
            ref.append(f'src/{file}:{ln}')
            prior=list(re.finditer(r'^(?:ASPELL|ACMD|SPECIAL)\((\w+)\)|^(?:static )?(?:void|int|bool|enum \w+)\s+(\w+)\([^;]*?\)\s*\{',text[:hit.start()],re.M))
            if prior: funcs.append(f'src/{file}:{next(g for g in prior[-1].groups() if g)}')
            line=text.splitlines()[ln-1].strip()
            if 'cooldown' in line:cool.append(f'src/{file}:{ln} '+line)
            if 'improve_ability_from_use' in line:improve.append(f'src/{file}:{ln}')
    classes={}
    for cm in re.finditer(r'spell_level\(\s*'+symbol+r'\s*,\s*(\w+)\s*,\s*(\d+)\s*\)',src['class.c']):
        classes[cm[1]]=int(cm[2])
    if 'i' in classes:
        level=classes.pop('i');classes.update({c:level for c in ('CLASS_MAGIC_USER','CLASS_CLERIC','CLASS_THIEF','CLASS_WARRIOR','CLASS_PALADIN','CLASS_BARD','CLASS_WARLOCK','CLASS_DRUID','CLASS_MYSTIC')})
    rows.append(dict(id=defs[symbol],symbol=symbol,name=name.strip('"'),type='spell' if defs[symbol]<=defs['MAX_SPELLS'] else 'skill',registration=f"src/spell_parser.c:{src['spell_parser.c'].count(chr(10),0,src['spell_parser.c'].find(m[0]))+1}",mechanism=mechanism,targets=target,position=pos,violent=violent,classes=classes,wearoff=msg,cost_min=mincost,cost_max=maxcost,cost_change=change,functions=sorted(set(funcs)),references=ref,cooldowns=sorted(set(cool)),improvement_hooks=sorted(set(improve))))
Path('ability_inventory_raw.json').write_text(json.dumps(rows,indent=2))
registered={r['symbol'] for r in rows}
print('Registered',len(rows),Counter(r['type'] for r in rows))
print('Duplicate registrations',[x for x,n in Counter(r['id'] for r in rows).items() if n>1])
print('Duplicate names',[x for x,n in Counter(r['name'] for r in rows).items() if n>1])
print('Unregistered definitions',[(k,v) for k,v in defs.items() if k not in registered and k.startswith(('SPELL_','SKILL_'))])
print('No native class',[r['symbol'] for r in rows if not r['classes']])
print('No improvement hooks',[r['symbol'] for r in rows if r['type']=='skill' and not r['improvement_hooks']])

