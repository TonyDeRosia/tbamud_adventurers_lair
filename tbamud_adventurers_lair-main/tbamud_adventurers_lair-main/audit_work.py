import re, json
from pathlib import Path
ROOT=Path(r'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED')
def mobs():
    active=set((ROOT/'lib/world/mob/index').read_text().split())
    rows=[]
    for p in sorted((ROOT/'lib/world/mob').glob('*.mob')):
        if p.name not in active: continue
        raw=p.read_bytes().decode('latin1')
        for m in re.finditer(r'^#(\d+)\r?\n(.*?)(?=^#\d+\r?$|^\$)',raw,re.M|re.S):
            v=int(m[1]); body=m[2]; fields=body.split('~',4)
            if len(fields)!=5: raise ValueError((p,v))
            profile=re.findall(r'^BodyProfile:\s*(\d+)',fields[4],re.M)
            rows.append(dict(vnum=v,file='lib/world/mob/'+p.name,keywords=fields[0].strip(),short=fields[1].strip(),long=fields[2].strip(),description=fields[3].strip(),profile=int(profile[0]) if profile else None,format=fields[4].strip().splitlines()[0]))
    assert len({r['vnum'] for r in rows})==len(rows)
    return rows
if __name__=='__main__':
    rows=mobs()
    Path('mob_inventory_raw.json').write_text(json.dumps(rows,indent=2))
    from collections import Counter
    print('Total',len(rows),'Explicit',sum(r['profile'] is not None for r in rows),'Formats',Counter(r['format'].split()[-1] for r in rows))
    Path('mob_names.txt').write_text('\n'.join(f"{r['vnum']}|{re.sub(r'@.', '',r['short'])}|{r['keywords']}" for r in rows))
