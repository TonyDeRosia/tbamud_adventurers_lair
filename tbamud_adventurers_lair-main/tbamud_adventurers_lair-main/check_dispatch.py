import re,json
from pathlib import Path
R=Path(r'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED')
rows=json.loads(Path('ability_inventory_raw.json').read_text())
def function(text,name):
    m=re.search(r'^(?:static )?(?:void|int|bool) '+name+r'\([^;]+?\)\s*\{',text,re.M)
    if not m:return ''
    start=m.end();depth=1;i=start
    # braces in strings aren't present in these dispatcher functions
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}');i+=1
    return text[start:i]
magic=(R/'src/magic.c').read_text();parser=(R/'src/spell_parser.c').read_text()
for row in rows:
    row['dispatch']=[]
    for flag in re.findall('MAG_[A-Z_]+',row['mechanism']):
        fun='call_magic' if flag=='MAG_MANUAL' else 'mag_'+flag[4:].lower()
        body=function(parser if flag=='MAG_MANUAL' else magic,fun)
        if row['symbol'] not in body:
            print('Missing dispatch?',row['symbol'],flag,fun)
        manual=re.search(r'case '+row['symbol']+r':\s*MANUAL_SPELL\((\w+)\)',body)
        row['dispatch'].append(manual[1] if manual else fun)
Path('ability_inventory_raw.json').write_text(json.dumps(rows,indent=2))
