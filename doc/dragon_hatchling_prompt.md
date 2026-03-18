# Dragon Hatchling Prompt

## NEW SKILLS / PASSIVES

- SKILL_THICK_SCALES
- SKILL_DRACONIC_INSTINCT
- SKILL_ELEMENTAL_AFFINITY
- SKILL_HOARD_SENSE
- SKILL_APEX_PREDATOR
- SKILL_APPRAISE_ENEMY

--------------------------------------------------

SKILL_APPRAISE_ENEMY
Type: active utility skill
Target: visible character in room
Cost: 8 move
Min level: 10
Cooldown: 2 rounds

Purpose:
  assess an enemy, player, mob, or NPC and reveal combat-relevant information
  in immersive text rather than exact raw numbers

Success check:
  use existing skill success logic
  success quality should scale by:
    user level
    learned skill percent
    target level
    target concealment or stealth if any
  do not reveal anything if target is not visible unless the user also has
  valid detection such as detect invis, detect hidden, or truesight

On failure:
  return a vague or partially incorrect impression
  examples:
    "You cannot get a clean read on $N."
    "Your appraisal of $N is uncertain."
    "You misjudge $N's capabilities."

On normal success, reveal:
  apparent level band:
    trivial
    weaker than you
    near your level
    stronger than you
    far stronger than you
  current health state:
    unwounded
    lightly hurt
    wounded
    badly wounded
    near collapse
  combat role impression:
    melee fighter
    agile skirmisher
    armored defender
    spellcaster
    support fighter
    summoner
    unclear
  visible condition flags if present:
    poisoned
    burning
    rooted
    stunned
    hasted
    slowed
    protected by magic
    shielded
    hidden by shadow
    undead
    summoned

On strong success, also reveal:
  rough offensive threat:
    low
    moderate
    high
    severe
  rough defensive threat:
    fragile
    steady
    durable
    extremely durable
  rough magical threat:
    none
    minor
    notable
    dangerous
    overwhelming
  visible active buffs and wards:
    examples:
      stoneskin
      barkskin
      mirror effects
      elemental wards
      death ward
      truesight
      bloodlust
      shadow armor
    only reveal effects that are actually active and detectable

On excellent success, also reveal:
  rough resistance profile if detectable:
    resistant to fire
    resistant to cold
    resistant to lightning
    resistant to acid
    resistant to death magic
    difficult to frighten
    difficult to restrain
  rough weakness profile if detectable:
    lightly armored
    vulnerable to disruption
    already weakened
    unstable under pressure
  whether the target appears:
    physically dominant
    magically dominant
    balanced
    exhausted
    hiding true power

Player target restrictions:
  against player characters, never reveal:
    exact hit points
    exact mana
    exact move
    exact level
    exact damage bonuses
    exact AC
    exact equipment stats
    exact resist percentages
  use rough descriptive categories only

Mob and NPC target rules:
  against mobs and NPCs, richer information is allowed, but still prefer
  descriptive bands over raw exact numbers unless a very high success result
  exists and your codebase already supports safe exact inspection

Messages:
  to_char: "You study $N with a predator's eye, appraising strengths and weaknesses."
  to_room: "$n studies $N with a cold, measuring stare."

Output format:
  use a compact multi-line appraisal report in plain text, for example:
    "You appraise $N:"
    "Power: stronger than you."
    "Condition: wounded."
    "Role: armored defender."
    "Threat: high physical danger."
    "Protection: magical shielding detected."
    "Weakness: slowed movement."

Implementation notes:
  use visible affects and existing combat stats where safe
  do not invent hidden stat systems
  if exact resistance and weakness inference is too invasive, infer only from:
    active ward affects
    visible defensive affects
    undead or summon flags
    current debuffs
  if success-tiered appraisal is too invasive, implement a two-tier model:
    failure
    success
    and note the limitation
