"""Render original mono location and vehicle recordings from oscillators and filtered noise."""
import argparse
import array
import math
import random
import sys
import wave
from pathlib import Path

RATE=24000
TAU=2*math.pi
DURATIONS={"Engine":2.0,"Tyres":3.0,"Horn":.53,"Shift":.19,"FootStone":.24,
           "FootGrit":.26,"Rain":7.0,"River":9.0,"Wind":8.0,"TeaBell":.83,"Contact":.34}
LOOPS={"Engine","Tyres","Rain","River","Wind"}

def sound(name, index, seconds):
    rng=random.Random(4613+index)
    slow=mid=prior=0.
    values=[]
    for i in range(int(RATE*seconds)):
        t=i/RATE
        noise=rng.uniform(-1,1)
        last=mid
        mid+=.16*(noise-mid)
        slow+=.019*(noise-slow)
        grit=mid-last
        if name=="Engine":
            # A measured four-stroke cadence at 900 rpm, with a distinct intake pulse on every second firing.
            phase=TAU*30*t
            v=sum(math.sin(phase*h+.13*math.sin(phase*.5)) / (h**1.10) for h in range(1,12))*.23
            v+=.07*math.sin(TAU*15*t)+mid*.10*(.45+.55*abs(math.sin(phase)))
        elif name=="Tyres":
            v=(.37*mid+.13*noise+.36*slow)*(.84+.16*math.sin(TAU*t/seconds))
        elif name=="Horn":
            v=sum(math.sin(TAU*freq*t*k)/(k*k) for freq in (384,486) for k in range(1,6))*.28
            v*=min(t/.018,1.)*min((seconds-t)/.072,1.)
        elif name in ("FootStone","FootGrit","Shift","Contact"):
            decay={"FootStone":22,"FootGrit":18,"Shift":34,"Contact":17}[name]
            weight={"FootStone":105,"FootGrit":89,"Shift":136,"Contact":55}[name]
            v=(.46*math.sin(TAU*(weight*t-38*t*t))+.52*mid+.18*noise)*math.exp(-decay*t)
            v+=.20*noise*math.exp(-38*abs(t-.082))
            if name=="FootGrit": v+=.22*grit*math.exp(-12*t)
            v*=min(t/.006,1.)
        elif name=="TeaBell":
            v=sum(math.sin(TAU*f*t)*a*math.exp(-t*d) for f,a,d in [(1260,.42,5.6),(3200,.19,10.),(3970,.11,13.)])*min(1.,t/.004)
        else:
            if name=="Rain":
                v=.25*noise+.52*mid+.23*slow
                if rng.random()<.0019: prior+=rng.uniform(.16,.38)
                prior*=.978
                v+=prior*math.sin(TAU*1830*t)
            elif name=="River":
                v=.62*slow+.32*mid*(.63+.37*math.sin(TAU*2*t/seconds))
                v+=.05*math.sin(TAU*(182*t+.8*math.sin(TAU*t/seconds)))*max(0.,math.sin(TAU*3*t/seconds))**10
            else:
                v=(.62*slow+.23*mid)*(.65+.23*math.sin(TAU*t/seconds)+.12*math.cos(TAU*3*t/seconds))
        values.append(v)
    if name in LOOPS:
        # Blend the trailing texture into the opening without introducing an audible loop boundary.
        n=int(.06*RATE)
        for i in range(n):
            f=i/n
            values[-n+i]=values[-n+i]*(1-f)+values[i]*f
        values=values[n:]
    peak=max(abs(v) for v in values)
    return array.array("h",(round(v*.81/max(.6,peak)*32767) for v in values))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out",type=Path)
    folder=parser.parse_args().out
    folder.mkdir(exist_ok=True,parents=True)
    for index,(name,seconds) in enumerate(DURATIONS.items()):
        pcm=sound(name,index,seconds)
        if sys.byteorder!="little": pcm.byteswap()
        with wave.open(str(folder/("S_"+name+".wav")),"wb") as dest:
            dest.setparams((1,2,RATE,0,"NONE","not compressed"))
            dest.writeframes(pcm.tobytes())

if __name__=="__main__":
    main()
