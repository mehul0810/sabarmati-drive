"""Prepare credited 2K Radiance panoramas for the offline sky material using the Python standard library."""
import argparse
import hashlib
import math
from pathlib import Path
import statistics
import struct
import zlib

SOURCES={"sunset":("T_SabarmatiEvening","b54af864f1868a59cdc391744532776c"),
         "day":("T_SabarmatiClouds","2eba3a4d7eeb23cbfbeca364c97e7980")}

def rows(f,w,h):
    for _ in range(h):
        header=f.read(4)
        if header!=bytes([2,2,w>>8,w%256]): raise ValueError("Unexpected Radiance scanline")
        channels=[]
        for _ in range(4):
            ch=bytearray()
            while len(ch)<w:
                c=f.read(1)
                if not c or c[0]==0: raise ValueError("Invalid run")
                if c[0]>128: ch.extend(f.read(1)*(c[0]-128))
                else: ch.extend(f.read(c[0]))
            if len(ch)!=w: raise ValueError("Invalid scanline length")
            channels.append(ch)
        yield list(zip(*channels))

def convert(file,name,md5,out):
    if hashlib.md5(file.read_bytes()).hexdigest()!=md5: raise ValueError("HDR source checksum mismatch: "+str(file))
    with file.open("rb") as f:
        while True:
            line=f.readline()
            if line in (b"\n",b"\r\n"): break
            if not line: raise ValueError("Missing HDR dimensions")
        top,h,side,w=f.readline().decode().split()
        if (top,side)!=("-Y","+X"): raise ValueError("Unexpected axes")
        w,h=int(w),int(h)
        pixels=list(rows(f,w,h))
    sample=[]; peak=(-1,0)
    for y,row in enumerate(pixels[:h//2]):
        for x in range(0,w,4):
            r,g,b,e=row[x]
            lum=(.2126*r+.7152*g+.0722*b)*2.**(e-136)
            peak=max(peak,(lum,x))
            if y%8==0 and y>h*.09 and lum>0: sample.append(lum)
    exposure=.64/statistics.median(sample)
    cache={}
    def tone(c,e):
        key=(c,e)
        if key not in cache:
            value=c*2.**(e-136)*exposure
            cache[key]=round(255*(value/(1+value))**(1/2.2))
        return cache[key]
    scan=bytearray()
    for row in pixels:
        shift=(peak[1]-w//2)%w
        scan.append(0)
        for r,g,b,e in row[shift:]+row[:shift]:
            scan.extend((tone(r,e),tone(g,e),tone(b,e)))
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
    image=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(scan,7))+chunk(b"IEND",b"")
    (out/(name+".png")).write_bytes(image)
    print(name,w,h,"solar azimuth centered",peak[1])

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("folder",type=Path); parser.add_argument("output",type=Path)
    a=parser.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    for src,(name,digest) in SOURCES.items(): convert(a.folder/(src+".hdr"),name,digest,a.output)
