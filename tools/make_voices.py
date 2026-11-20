"""v4.5: alle voicelines van Small Giveaway Bike 1.

Maakt met Kokoro (lokaal, de kit in Downloads/zach/kit) per regel Resources/vo_<key>.mp3 en
Game/voice_data.c + Game/voice_data.h (ondertitels, spreker, duur, welke regels bij welke missie horen).

    <kit>/.venv/Scripts/python tools/make_voices.py          (alleen nieuwe/gewijzigde regels)
    <kit>/.venv/Scripts/python tools/make_voices.py --all    (alles opnieuw)
"""
import hashlib, json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
KIT = os.environ.get('SGB_KOKORO', os.path.join(os.path.expanduser('~'), 'Downloads', 'zach', 'kit', 'models'))

# ------------------------------------------------------------------ sprekers (naam in beeld, Kokoro-stem, snelheid)
SPK = [
    ('you',   'YOU',          'am_liam',    1.08),
    ('gg',    'GIVEAWAY GUY', 'am_eric',    1.10),
    ('rosa',  'MAMA ROSA',    'af_bella',   1.05),
    ('coach', 'COACH DRE',    'am_fenrir',  1.08),
    ('rico',  'RICO',         'am_puck',    1.12),
    ('bigt',  'BIG T',        'am_onyx',    1.00),
    ('nina',  'NINA',         'af_nova',    1.10),
    ('cop',   'POLICE',       'bm_george',  1.08),
    ('clerk', 'CLERK',        'am_echo',    1.08),
    ('gunny', 'BLOCK ARMS',   'bm_daniel',  1.02),
    ('burg',  'BURGER GIRL',  'af_nicole',  1.08),
    ('drip',  'DRIP',         'af_heart',   1.08),
    ('bling', 'BLING',        'bf_emma',    1.05),
    ('gym',   'GYM BRO',      'am_adam',    1.10),
    ('ped',   'PEDESTRIAN',   'af_sarah',   1.12),
]
SPK_ID = {s[0]: i for i, s in enumerate(SPK)}

# ------------------------------------------------------------------ missies
# intro: de cutscene voor de missie (afwisselend contact en jij)
# step: {n: regel} = gezegd zodra je bij checkpoint n bent aangekomen (n = nieuwe stap; bij anyOrder = aantal verzameld)
# alarm: als het alarm afgaat; open: als de kluis/kassa leeg is; pass/fail: aan het eind (via de telefoon)
M = [
  dict(t='WELCOME RIDE', who='gg', intro=[
      ('gg', "Yo! Welcome to the block! I'm the Giveaway Guy, and today is your lucky day."),
      ('gg', "See that red bike? That's the Giveaway Bike. It's yours. Free. No catch."),
      ('you', "For real? Just like that?"),
      ('gg', "Just like that. Hop on, follow the markers, and show me what you got!")],
    step={1: ('gg', "Nice! Now ride to the marker."), 3: ('gg', "Look at you go! Almost there."), 4: ('gg', "Bring it home to the plaza!")},
    pas=('gg', "That's how you ride! Welcome to the city, superstar."), fail=('gg', "Too slow, my friend. Try again!")),
  dict(t='PIZZA PANIC', who='rosa', intro=[
      ('rosa', "Ay, finally! My delivery boy quit, and I have three hot pizzas going cold!"),
      ('you', "Pizza delivery? I can do that."),
      ('rosa', "Three houses in the suburb. Fast, fast! Nobody likes a cold pizza.")],
    step={1: ('rosa', "One down! Two more, go go go!"), 2: ('rosa', "Last one! Do not drop it!")},
    pas=('rosa', "Bellissimo! They loved it. Here, take your money."), fail=('rosa', "Cold pizza! Mamma mia, what a disaster.")),
  dict(t='STREET SPRINT', who='coach', intro=[
      ('coach', "So you think you're fast on that bike? Let's find out."),
      ('you', "Name the route, coach."),
      ('coach', "Checkpoints all over the streets. Beat the clock and you earn my respect."),
      ('coach', "Ready? Three, two, one... go!")],
    step={3: ('coach', "Halfway! Keep those legs moving!")},
    pas=('coach', "Now that's a sprint! You've got real speed, kid."), fail=('coach', "Out of time. Go train and come back.")),
  dict(t='JOYRIDE RETURN', who='rico', intro=[
      ('rico', "Bro, bro, bro. Somebody took my red car. My red car!"),
      ('you', "Calm down. Where is it?"),
      ('rico', "Parking lot, last I heard. Grab it and bring it back to the garage. Not a scratch!")],
    step={1: ('rico', "You got it! Now drive it to the garage. Gently, please!")},
    pas=('rico', "My baby is home! You're a legend, bro."), fail=('rico', "Where's my car, man? Come on!")),
  dict(t='THE BIG GIVEAWAY', who='gg', intro=[
      ('gg', "Big day today! The biggest giveaway this city has ever seen!"),
      ('gg', "But I dropped five gift boxes all over town. Long story."),
      ('you', "Let me guess. You want me to find them."),
      ('gg', "Find them, and bring them to the stage in the park. The crowd is waiting!")],
    step={2: ('gg', "Two boxes! Keep looking!"), 4: ('gg', "One more box out there!"), 5: ('gg', "You got them all! Now to the park stage!")},
    pas=('gg', "The crowd goes wild! Best giveaway ever!"), fail=('gg', "The show's over... we ran out of time.")),
  dict(t='PARK RUN', who='coach', intro=[
      ('coach', "No bikes, no cars. Today you use your own two feet."),
      ('you', "Running? Seriously?"),
      ('coach', "Seriously. The park course. Hit every checkpoint. Let's move!")],
    step={4: ('coach', "Breathe! Don't stop now!")},
    pas=('coach', "That's real cardio. I'm proud of you."), fail=('coach', "Out of breath and out of time.")),
  dict(t='TAXI RUSH', who='rico', intro=[
      ('rico', "My cousin's taxi company needs a driver today. You can drive, right?"),
      ('you', "Can I drive? Watch me."),
      ('rico', "Pick up the passengers, drop them where they want. Easy money.")],
    step={1: ('ped', "Hi! East side, please. And hurry, I'm late!"), 2: ('ped', "Thanks! Here's a tip."),
          3: ('ped', "Last ride? North-west corner, driver.")},
    pas=('rico', "Five stars, bro! My cousin says you're hired."), fail=('rico', "The passengers walked away, man.")),
  dict(t='ALLEY SHORTCUT', who='nina', intro=[
      ('nina', "Hey. I need this package across town. No questions asked."),
      ('you', "What's in it?"),
      ('nina', "I said no questions. Take the back alleys on your bike. Stay out of sight.")],
    step={5: ('nina', "Halfway there. Keep to the alleys.")},
    pas=('nina', "Delivered. Smooth. I'll call you again."), fail=('nina', "Too slow. The buyer left.")),
  dict(t='COP DODGE', who='rico', intro=[
      ('rico', "Okay, bad news. The car you're in? Police think it's stolen."),
      ('you', "Is it?"),
      ('rico', "Technically... Look, just lose the cops! Drive!")],
    pas=('rico', "You lost them! Bro, you're crazy good."), fail=('rico', "They got you, man.")),
  dict(t='GRAND TOUR', who='rico', intro=[
      ('rico', "Wanna see the whole city? The ring road goes all the way around."),
      ('you', "A road trip?"),
      ('rico', "A fast road trip. Beat the clock, and the money is yours.")],
    step={4: ('rico', "Halfway around the city! Keep it floored!")},
    pas=('rico', "The full tour! That was beautiful."), fail=('rico', "The clock won this time.")),
  dict(t='THE BIG HEIST', who='bigt', intro=[
      ('bigt', "So. You're the one everybody's talking about."),
      ('you', "Depends on what they're saying."),
      ('bigt', "They say you're fast, and you don't panic. Good. The Giveaway Bank has a vault full of cash."),
      ('bigt', "Crack it. Lose the cops. Bring the money to the garage. Don't make me regret this.")],
    alarm=('bigt', "That's the alarm! Keep drilling, don't you dare leave!"),
    open=('bigt', "Vault's open! Now get out of there and lose those cops!"),
    pas=('bigt', "Clean job. You just made a lot of money. Welcome to the family."), fail=('bigt', "You blew it. Get out of my sight.")),
  dict(t='NIGHT DELIVERY', who='nina', intro=[
      ('nina', "Five shops need their parcels tonight. Before they close."),
      ('you', "Another no-questions package?"),
      ('nina', "Just parcels this time. Take your bike. Go.")],
    step={3: ('nina', "Three down, two to go.")},
    pas=('nina', "All delivered. You're reliable. I like that."), fail=('nina', "The shops are closed. Too late.")),
  dict(t='CASH COLLECTOR', who='gg', intro=[
      ('gg', "Okay, don't laugh. I dropped six bags of cash around town."),
      ('you', "Six bags? How?"),
      ('gg', "Hole in the truck. Collect them all and bring them to the plaza. You get a cut!")],
    step={3: ('gg', "Three bags! You're a natural."), 6: ('gg', "That's all of them! Now to the plaza!")},
    pas=('gg', "Every dollar is back! You're the best."), fail=('gg', "Somebody else found my money...")),
  dict(t='DOWNTOWN DASH', who='coach', intro=[
      ('coach', "Downtown. On foot. Against the clock."),
      ('you', "Again with the running?"),
      ('coach', "Again with the running. Champions don't complain. Go!")],
    step={4: ('coach', "Push it! Halfway there!")},
    pas=('coach', "Fastest dash I've seen all year!"), fail=('coach', "Not fast enough. Again!")),
  dict(t='SPEED TRAP', who='rico', intro=[
      ('rico', "The long roads are empty tonight. Perfect for a race."),
      ('you', "How fast are we talking?"),
      ('rico', "As fast as that car can go. Floor it!")],
    step={3: ('rico', "Faster, faster!")},
    pas=('rico', "You broke every speed record in town!"), fail=('rico', "Too slow, bro. Get a faster car.")),
  dict(t='CORNER STORE STICK-UP', who='bigt', intro=[
      ('bigt', "Small job. The bike shop keeps cash in the register."),
      ('you', "And you want it."),
      ('bigt', "I want you to take it. Empty the register, lose the cops, hide at the garage.")],
    alarm=('clerk', "Help! Somebody call the police!"),
    open=('bigt', "Got the cash? Now run!"),
    pas=('bigt', "Easy money. Nice work."), fail=('bigt', "You got sloppy.")),
  dict(t='GETAWAY DRIVER', who='bigt', intro=[
      ('bigt', "My crew is hitting the bank. They need a driver."),
      ('you', "I'm your driver."),
      ('bigt', "Pick them up at the bank, outrun the cops, and drop them at the hideout.")],
    step={1: ('nina', "Drive! Drive! Go, go, go!")},
    alarm=('cop', "All units, bank robbery in progress!"),
    pas=('bigt', "My crew is safe. You drive like a demon."), fail=('bigt', "The crew got caught. That's on you.")),
  dict(t='MOST WANTED', who='bigt', intro=[
      ('bigt', "Somebody told the cops you're the guy from the bank job."),
      ('you', "So what now?"),
      ('bigt', "Now? Three stars and the whole police force. Shake them off. Good luck, kid.")],
    pas=('bigt', "They lost you. Nobody can catch you now."), fail=('bigt', "They got you. Too bad.")),
  dict(t='TOUR DE STAD', who='coach', intro=[
      ('coach', "This is it. The big one. A full lap of the city on your bike."),
      ('you', "In how long?"),
      ('coach', "Four minutes. Every checkpoint. This is your Tour de Stad!")],
    step={5: ('coach', "Halfway! Don't you dare slow down!"), 9: ('coach', "Last checkpoint! Give it everything!")},
    pas=('coach', "Champion of the city! I knew you had it in you!"), fail=('coach', "So close. Rest up and try again.")),
  dict(t='GOLD RESERVE HEIST', who='bigt', intro=[
      ('bigt', "Forget cash. There's gold under that bank. A whole reserve."),
      ('you', "Gold? That's a big job."),
      ('bigt', "The biggest. Drill it, grab a car, drive to the corners and lose the whole alarm."),
      ('you', "Let's do it.")],
    step={2: ('bigt', "Switch it up! Head to the south-west corner!"), 3: ('bigt', "Now hide the gold at the garage!")},
    alarm=('cop', "Alarm at the Giveaway Bank! All units respond!"),
    open=('bigt', "The gold is yours! Now find a car, fast!"),
    pas=('bigt', "Gold bars in my garage. You're a legend."), fail=('bigt', "The gold is gone. So is my patience.")),
  dict(t='THE ULTIMATE HEIST', who='bigt', intro=[
      ('bigt', "This is the job of a lifetime. Register, vault, the whole city."),
      ('you', "Everything at once?"),
      ('bigt', "Everything. Rob the bike shop to warm up, then drill the main vault."),
      ('bigt', "Then you drive every corner of this city and lose every cop. Ready?"),
      ('you', "Born ready.")],
    step={1: ('bigt', "Warm-up done. Now the main vault!"), 3: ('bigt', "Next corner! Keep moving!"),
          6: ('bigt', "Now lose them all and get to the garage!")},
    alarm=('cop', "Every unit, the Giveaway Bank is being robbed!"),
    open=('bigt', "Vault's empty! Get a fast car!"),
    pas=('bigt', "The ultimate heist. You did it. You're the king of this city."), fail=('bigt', "So close to greatness.")),
  dict(t='THE IMPOSSIBLE JOB', who='bigt', intro=[
      ('bigt', "They call this the impossible job. Nobody has ever finished it."),
      ('you', "There's a first time for everything."),
      ('bigt', "Two robberies. The biggest vault drill ever. A full city chase. And then you go back in for the gold."),
      ('nina', "He's crazy. And so are you if you say yes."),
      ('you', "Yes.")],
    step={1: ('bigt', "Now drill the vault. Sixty seconds. Do not leave!"), 2: ('bigt', "Steal a car! South-east corner!"),
          8: ('bigt', "Back into the vault for the gold!"), 9: ('bigt', "Last step! Lose every single cop!")},
    alarm=('cop', "Code red at the Giveaway Bank! Every unit!"),
    open=('bigt', "Got it! Move, move!"),
    pas=('bigt', "Impossible? Not for you. Nobody will ever forget this."), fail=('bigt', "Impossible after all.")),
]

# ------------------------------------------------------------------ losse momenten (willekeurig een van de varianten)
BARKS = {
    'WANTED':     [('you', "Cops! I gotta move!"), ('you', "Oh no, not the police!"), ('cop', "Stop right there!"), ('cop', "Pull over, now!")],
    'LOSTCOPS':   [('you', "I think I lost them."), ('you', "Too easy."), ('you', "Bye bye, police.")],
    'BUSTED':     [('cop', "You're under arrest!"), ('cop', "Hands where I can see them!")],
    'WASTED':     [('you', "Ugh... I need a doctor."), ('you', "That hurt.")],
    'HURT':       [('you', "Ow!"), ('you', "Hey, watch it!"), ('you', "That's gonna leave a mark.")],
    'CARIN':      [('you', "Nice ride."), ('you', "Let's roll."), ('you', "Buckle up.")],
    'BIKEIN':     [('you', "Time to ride."), ('you', "Let's go!")],
    'CASH':       [('you', "Money!"), ('you', "Cha-ching!"), ('you', "Don't mind if I do.")],
    'CRASH':      [('you', "Whoa!"), ('you', "Oops."), ('ped', "Hey! Learn to drive!")],
    'SHOOT':      [('ped', "He's got a gun! Run!"), ('ped', "Help! Somebody help!")],
    'PASS':       [('you', "Easy money."), ('you', "Another one done.")],
    'FAIL':       [('you', "Ugh, so close."), ('you', "Next time.")],
    'STORE_GUN':  [('gunny', "Welcome to Block Arms. Look, don't touch. Unless you're buying."), ('gunny', "Need something with a little more bang?")],
    'STORE_BURGER':[('burg', "Hi! Welcome to Giveaway Burger! What can I get you?"), ('burg', "Hungry? You came to the right place!")],
    'STORE_GYM':  [('gym', "Yo, welcome to Block Gym! Let's get those gains!"), ('gym', "No pain, no gain, bro!")],
    'STORE_DRIP': [('drip', "Welcome to Drip! You need a fresh look, honey."), ('drip', "Ooh, let's get you some style!")],
    'STORE_BLING':[('bling', "Welcome to Bling. Only the finest for you, darling."), ('bling', "Gold, diamonds, luxury. Take a look.")],
    'STORE_MART': [('clerk', "Welcome to Giveaway Mart. Let me know if you need anything."), ('clerk', "Hey there. Open twenty-four seven.")],
    'ROB_YOU':    [('you', "Empty the register! Now!"), ('you', "This is a robbery! Hand it over!")],
    'ROB_CLERK':  [('clerk', "Okay, okay! Take it! Don't hurt me!"), ('clerk', "Please! Just take the money!")],
    'ROB_EMPTY':  [('clerk', "There's nothing left! You took it all!")],
    'BUY':        [('you', "I'll take it."), ('you', "Sold.")],
    'HOME':       [('you', "Home sweet home."), ('you', "Finally, some peace and quiet.")],
    'SKIP_NONE':  [('you', "Alright.")],
}
BARK_ORDER = list(BARKS.keys())

# ------------------------------------------------------------------ regels verzamelen
lines = []   # (key, spk, text)
def add(key, sp_text):
    sp, text = sp_text
    lines.append((key, SPK_ID[sp], text))
    return len(lines) - 1

mis = []
for i, m in enumerate(M):
    e = dict(intro=[add('m%02d_i%d' % (i, k), l) for k, l in enumerate(m['intro'])],
             step={n: add('m%02d_s%d' % (i, n), l) for n, l in m.get('step', {}).items()},
             alarm=add('m%02d_al' % i, m['alarm']) if 'alarm' in m else -1,
             open=add('m%02d_op' % i, m['open']) if 'open' in m else -1,
             pas=add('m%02d_ok' % i, m['pas']), fail=add('m%02d_no' % i, m['fail']), who=SPK_ID[m['who']])
    mis.append(e)
bark = {b: [add('b_%s%d' % (b.lower(), k), l) for k, l in enumerate(v)] for b, v in BARKS.items()}

# ------------------------------------------------------------------ audio maken
def main():
    force = '--all' in sys.argv
    man_path = os.path.join(HERE, 'voices_manifest.json')
    man = json.load(open(man_path)) if os.path.exists(man_path) else {}
    todo = []
    for key, sp, text in lines:
        h = hashlib.md5(('%s|%s|%s' % (SPK[sp][2], SPK[sp][3], text)).encode()).hexdigest()[:12]
        out = os.path.join(ROOT, 'Resources', 'vo_%s.mp3' % key)
        if force or man.get(key, {}).get('h') != h or not os.path.exists(out):
            todo.append((key, sp, text, h, out))
    if todo:
        import numpy as np, soundfile as sf
        from kokoro_onnx import Kokoro
        k = Kokoro(os.path.join(KIT, 'kokoro-v1.0.onnx'), os.path.join(KIT, 'voices-v1.0.bin'))
        for n, (key, sp, text, h, out) in enumerate(todo):
            a, sr = k.create(text, voice=SPK[sp][2], speed=SPK[sp][3], lang='en-us')
            a = np.asarray(a, dtype='float32')
            # stilte aan begin/eind wegknippen, kort staartje laten
            nz = np.nonzero(np.abs(a) > 0.01)[0]
            if len(nz): a = a[max(0, nz[0] - int(sr * 0.03)): min(len(a), nz[-1] + int(sr * 0.12))]
            peak = float(np.max(np.abs(a))) or 1.0
            a = a * (0.89 / peak)
            sf.write(out, a, sr, format='MP3', compression_level=0.75)
            man[key] = dict(h=h, dur=round(len(a) / sr, 3))
            print('[%d/%d] %s %s: %s' % (n + 1, len(todo), key, SPK[sp][1], text), flush=True)
        json.dump(man, open(man_path, 'w'), indent=0, sort_keys=True)
    # oude bestanden die niet meer in het script staan opruimen
    keys = {l[0] for l in lines}
    for f in os.listdir(os.path.join(ROOT, 'Resources')):
        if f.startswith('vo_') and f.endswith('.mp3') and f[3:-4] not in keys:
            os.remove(os.path.join(ROOT, 'Resources', f))
    write_c(man)
    tot = sum(os.path.getsize(os.path.join(ROOT, 'Resources', 'vo_%s.mp3' % l[0])) for l in lines)
    print('%d regels, %.1f MB' % (len(lines), tot / 1e6))

def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def write_c(man):
    h = ['/* GEMAAKT DOOR tools/make_voices.py - niet met de hand aanpassen */',
         '#ifndef SGB_VOICE_DATA_H', '#define SGB_VOICE_DATA_H', '',
         'typedef struct { const char *key; int spk; const char *text; float dur; } VoLine;',
         'typedef struct { short intro[6]; short nintro; short step[11]; short alarm, open, pass, fail; short who; } VoMission;',
         '#define VO_SPEAKERS %d' % len(SPK), '#define VO_LINES %d' % len(lines), '#define VO_MISSIONS %d' % len(mis),
         'enum { ' + ', '.join('VB_' + b for b in BARK_ORDER) + ', VB_COUNT };',
         '#define VO_BARK_MAX 4',
         'extern const VoLine VO[VO_LINES];', 'extern const char *const VO_SPK_NAME[VO_SPEAKERS];',
         'extern const VoMission VO_MIS[VO_MISSIONS];', 'extern const short VO_BARK[VB_COUNT][VO_BARK_MAX];',
         'enum { ' + ', '.join('VS_' + s[0].upper() for s in SPK) + ' };', '', '#endif', '']
    c = ['/* GEMAAKT DOOR tools/make_voices.py - niet met de hand aanpassen */', '#include "voice_data.h"', '',
         'const char *const VO_SPK_NAME[VO_SPEAKERS] = {' + ', '.join(cstr(s[1]) for s in SPK) + '};', '',
         'const VoLine VO[VO_LINES] = {']
    for key, sp, text in lines:
        c.append('    {%s, %d, %s, %.3ff},' % (cstr(key), sp, cstr(text), man[key]['dur']))
    c += ['};', '', 'const VoMission VO_MIS[VO_MISSIONS] = {']
    for e in mis:
        intro = (e['intro'] + [-1] * 6)[:6]
        step = [e['step'].get(n, -1) for n in range(11)]
        c.append('    {{%s}, %d, {%s}, %d, %d, %d, %d, %d},' % (', '.join(map(str, intro)), len(e['intro']), ', '.join(map(str, step)),
                                                         e['alarm'], e['open'], e['pas'], e['fail'], e['who']))
    c += ['};', '', 'const short VO_BARK[VB_COUNT][VO_BARK_MAX] = {']
    for b in BARK_ORDER:
        v = (bark[b] + [-1] * 4)[:4]
        c.append('    {%s},   /* %s */' % (', '.join(map(str, v)), b))
    c += ['};', '']
    open(os.path.join(ROOT, 'Game', 'voice_data.h'), 'w', newline='\n').write('\n'.join(h))
    open(os.path.join(ROOT, 'Game', 'voice_data.c'), 'w', newline='\n').write('\n'.join(c))

if __name__ == '__main__':
    main()
