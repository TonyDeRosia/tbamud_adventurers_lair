import re,json
from pathlib import Path
from collections import Counter
rows=json.loads(Path('mob_inventory_raw.json').read_text())
names=['None','Humanoid','Quadruped','Serpent','Avian','Arachnid','Dragon','Horned Humanoid','Tailed Humanoid','Insectoid','Winged Insectoid','Construct','Amorphous','Fish','Plant','Centauroid','Crustacean','Winged Quadruped','Tentacled','Bat']
rules={
0:r'ghost|spectre|specter|wraith|elemental|swarm|feeling|apparition|banshee|phantom|aerial servant',
7:r'minotaur|satyr',8:r'werewolf|lizardman|lizardfolk|ratman',
15:r'centaur',16:r'crab|lobster|crayfish',
11:r'golem|construct|android|robot|automaton|animated (?:training )?(?:dummy|husk)|animated armor',
12:r'slime|slimeball|ooze|blob',
14:r'plant|treant|ficus|flytrap|vine|oaken tree|ancient tree',
5:r'spider|spiderling|scorpion|tarantula',
10:r'moth|wasp|cicada|mosquito|hornet|butterfly|dragonfly|bee|fishfly',
9:r'beetle|ant|cockroach|mantis|silverfish|insect|insects',
13:r'fish|shark|trout|barracuda|swordfish|piranha|salmon|cavefish',
3:r'snake|serpent|worm|eel|larva|grub|maggot',
4:r'bird|eagle|hawk|falcon|owl|raven|crow|vulture|condor|kestrel|heron|chicken|rooster|duck|goose|swan|pheasant|pigeon|sparrow|parrot|penguin|pelican|seagull',
6:r'dragon|drake',
2:r'wolf|wolves|dog|hound|terrier|puppy|mutt|cat|kitten|lion|tiger|panther|leopard|cheetah|bear|horse|mare|stallion|pony|colt|deer|stag|doe|elk|moose|boar|pig|sow|hog|cow|bull|calf|ox|goat|sheep|ram|lamb|rat|mouse|mice|rabbit|hare|fox|badger|weasel|ferret|otter|raccoon|squirrel|chipmunk|beaver|elephant|rhinoceros|hippopotamus|camel|donkey|mule|lizard|crocodile|alligator|turtle|tortoise|frog|toad|iguana|salamander',
1:r'humanoid|human|elf|elven|elves|dwarf|dwarven|drow|gnome|halfling|hobbit|orc|goblin|hobgoblin|ogre|troll|giant|zombie|skeleton|vampire|ghoul|mummy|lich|man|woman|boy|girl|child|baby|lady|gentleman|warrior|knight|paladin|mage|magi|wizard|sorcerer|witch|cleric|priest|priestess|druid|thief|rogue|assassin|guard|gateguard|guildguard|guildmaster|soldier|captain|sergeant|lieutenant|corporal|general|commander|sentry|mercenary|bandit|brigand|pirate|sailor|ranger|archer|hunter|peasant|farmer|worker|miner|merchant|trader|shopkeeper|bartender|innkeeper|innkeep|waiter|waitress|barmaid|barkeep|blacksmith|weaponsmith|armorer|armourer|jeweler|healer|sage|scholar|scribe|monk|nun|noble|king|queen|prince|princess|duke|duchess|count|countess|baron|baroness|lord|mayor|butler|maid|servant|manservant|slave|beggar|bum|drunk|citizen|townsman|townswoman|baker|butcher|cook|fisherman|pickpocket|peddler|statesman|stateswoman|midwife|banker|questmaster|recruit|patrolman|hoodlum|punk|leper|gypsy|nomad|adventurer|acolyte|apprentice|student|teacher|trainer|bard|musician|dancer|jester|clown|fool|pilgrim|refugee|prisoner|jailer|executioner|smuggler|swordsman|swordswoman|axeman|warpriest|matron|mother|father|daughter|son|wife|husband'
}
rules[1]+=r'|kid|saleswoman|salesman|leatherworker|postmaster|postmistress|steward|clerk|archivist|quartermaster|instructor|tutor|registrar|lecturer|provost|alchemist|herbalist|warden|guardian|sentinel|squire|serf|damsel|troubadour|entertainer|minstrel|slaver|chief|chieftain|clansman|archbishop|bishop|sorceress|commando|officer|bodyguard|empress|emperor|artist|journalist|chef|janitor|librarian|judge|gladiator|spectator|vendor|grocer|doctor|dentist|senator|bailiff|defendant|looter|spy|commissar|hermit|huntress|gambler|biker|carpenter|philosopher|gibberling|infantryman|woodcutter|stablehand|coach|teller|councillor|scout|scoutmaster|fencer|murderer|herald|astrologer'
rules[2]+=r'|german shepherd|collie|bobcat|coyote|buck|possum|skunk|lynx|heifer|groundhog|woodchuck|wolverine|piglet|cougar|marmot|warhorse|mole'
rules[4]+=r'|dove|turkey|meadowlark|lark|harrier|chick|taildove'
rules[9]+=r'|bedbug'
rules[11]+=r'|terminator|clockwork'
rules[17]=r'griffin|griffon|hippogriff|pegasus'
rules[18]=r'squid|kraken|octopus'
rules[19]=r'bat'
rules[0]+=r'|bodyless soul|will-o-wisp|swirling mist'
def hits(p,t):return re.findall(r'\b(?:'+p+r')\b',t,re.I)
overrides={0:(0,'Invisible DG trigger host, not a creature'),34000:(11,'Crystal plate guardian assembled into a humanoid outline'),34002:(11,'Construct of fitted facets with four hands'),34003:(11,'Heart-prism crystal guardian'),34013:(10,'Winged insect broodmother'),34014:(3,'Legless larval body'),34016:(9,'Beetle, not arachnid; abbreviated anatomy'),34019:(0,'Swarm has no single harvestable body'),5:(1,'Human Russell namesake'),6:(1,'Human Russell namesake'),8:(2,'Named terrier'),12:(2,'Named terrier'),2544:(1,'Flesh golem preserves humanoid flesh anatomy')}
overrides.update({34001:(10,'Six legs and glass wings explicitly described'),34014:(None,'Larva explicitly has many tiny legs; neither serpent nor six-legged adult fits'),34016:(None,'Eight-legged horned beetle: not an arachnid, standard insect profile has six legs'),34007:(None,'Gastropod has organized organs; not amorphous or conventional serpent')})
for r in rows:
    clean=lambda t: re.sub(r'@.', '',t).lower()
    short=clean(r['short']); kw=clean(r['keywords']); desc=clean(r['long']+' '+r['description'])
    matches=[p for p,pat in rules.items() if sum(bool(hits(pat,t)) for t in (short,kw,desc))>=2]
    # Anatomical nouns supersede occupational adjectives, but competing animal
    # shapes require review. Spider common names override e.g. "wasp spider".
    if 5 in matches: matches=[5]
    elif 11 in matches: matches=[11]
    elif 0 in matches: matches=[0]
    elif 7 in matches: matches=[7]
    elif 8 in matches: matches=[8]
    elif 15 in matches: matches=[15]
    elif 12 in matches: matches=[12]
    elif 14 in matches: matches=[14]
    elif 10 in matches: matches=[10]
    elif 17 in matches: matches=[17]
    elif 18 in matches: matches=[18]
    elif 19 in matches: matches=[19]
    elif len(matches)>1: matches=[p for p in matches if p!=1]
    if r['vnum'] in overrides:
        p,why=overrides[r['vnum']]; confidence='high'; reason=why
    elif len(matches)==1:
        p=matches[0];confidence='medium';reason='Identity corroborated in at least two description fields: '+', '.join(sorted(set(hits(rules[p],short+' '+kw))))
    elif not matches and hits(r'^(?:this|the|a|an) (?:\w+[ ,]+){0,5}(?:man|woman|human|elf|dwarf|humanoid)\b',desc) and hits(r'he|she|his|her|hands|arms|legs|robe|shirt|dress|wears|wearing',desc):
        p=1;confidence='medium';reason='Description directly identifies a humanoid with corroborating personal or bodily detail'
    else:
        p=None;confidence='review';reason='Conflicting identities' if matches else 'No corroborated supported anatomy'
    if p is None: confidence='review'
    r.update(recommended=p,confidence=confidence,reason=reason)
Path('mob_classified.json').write_text(json.dumps(rows,indent=2))
print(Counter(names[r['recommended']] if r['recommended'] is not None else 'REVIEW' for r in rows))
Path('mob_review.txt').write_text('\n'.join(f"{r['vnum']}|{r['short']}|{r['keywords']}|{r['description']}" for r in rows if r['recommended'] is None))
