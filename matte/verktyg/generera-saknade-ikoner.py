#!/usr/bin/env python3
"""Skapar reservikoner i en separat byggmapp (aldrig i källkodsmappen).

Kräver bara Python 3-standardbiblioteket. Originalikonerna kopieras senare
före reserven av expandera.py och ersätts aldrig av bokstavsikoner.
Beroende på referenser skapas 96x96 PNG-filer.
"""
from pathlib import Path
import re, sys, struct, zlib, hashlib

FONT = {
'A':[14,17,17,31,17,17,17],'B':[30,17,17,30,17,17,30],
'C':[14,17,16,16,16,17,14],'D':[30,17,17,17,17,17,30],
'E':[31,16,16,30,16,16,31],'F':[31,16,16,30,16,16,16],
'G':[14,17,16,23,17,17,14],'H':[17,17,17,31,17,17,17],
'I':[14,4,4,4,4,4,14],'J':[7,2,2,2,18,18,12],
'K':[17,18,20,24,20,18,17],'L':[16,16,16,16,16,16,31],
'M':[17,27,21,21,17,17,17],'N':[17,25,21,19,17,17,17],
'O':[14,17,17,17,17,17,14],'P':[30,17,17,30,16,16,16],
'Q':[14,17,17,17,21,18,13],'R':[30,17,17,30,20,18,17],
'S':[15,16,16,14,1,1,30],'T':[31,4,4,4,4,4,4],
'U':[17,17,17,17,17,17,14],'V':[17,17,17,17,10,10,4],
'W':[17,17,17,21,21,21,10],'X':[17,17,10,4,10,17,17],
'Y':[17,17,10,4,4,4,4],'Z':[31,1,2,4,8,16,31],
'0':[14,17,19,21,25,17,14],'1':[4,12,4,4,4,4,14],
'2':[14,17,1,2,4,8,31],'3':[30,1,1,14,1,1,30],
'4':[2,6,10,18,31,2,2],'5':[31,16,16,30,1,1,30],
'6':[14,16,16,30,17,17,14],'7':[31,1,2,4,8,8,8],
'8':[14,17,17,14,17,17,14],'9':[14,17,17,15,1,1,14],
'?':[14,17,1,2,4,0,4],'+':[0,4,4,31,4,4,0],
'-':[0,0,0,31,0,0,0],'%':[17,18,4,8,19,17,0],
}
PALETTE=[(99,73,230),(29,151,156),(210,116,55),(60,138,219),
         (100,156,79),(183,81,155),(190,135,52),(97,102,192)]
EXTRA={
    'fragetecken','ratt','fel','kulram','bok','karta','stjarna','pokal','tarning',
    'apple','banan','apelsin','paron','jordgubbe','morot','kaka','mynt','strumpor',
    'hjarta','raket','linjal','koordinatsystem','passare','gradskiva','tallinje',
    'pusselbit','gloidlampa','plus','minus','procent','halvor','fjardedelar',
    'vag','triangel','lika-med','kuber-gula','stapeltorn','linjediagram','kalender',
    'cirkel','femhorning','sexhorning','romb','kvadrat','rektangel','oval',
    'kub','klot','kon','cylinder','pyramid','parallellogram','trapets'
}
SYMBOL={'plus':'+','procent':'%','fragetecken':'?','minus':'-','ratt':'OK',
        'fel':'X','fjardedelar':'14','halvor':'12','tarning':'6','stjarna':'ST'}

def png_encode(rows, width, height):
    raw=b''.join(b'\x00'+bytes(row) for row in rows)
    def chunk(tag,data):
        return struct.pack('!I',len(data))+tag+data+struct.pack('!I',zlib.crc32(tag+data)&0xffffffff)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b'')

def draw(name):
    W=H=96
    pix=bytearray(W*H*4)
    def put(x,y,c):
        if x<0 or y<0 or x>=W or y>=H:return
        k=(y*W+x)*4;pix[k:k+4]=bytes(c)
    h=hashlib.sha256(name.encode()).digest()[0]
    base=PALETTE[h%len(PALETTE)]
    for y in range(H):
        for x in range(W):
            dx=max(0,12-x,x-83);dy=max(0,10-y,y-82)
            if dx*dx+dy*dy<100:
                b=max(0,(y-10))/80
                c=tuple(int(v*(0.96-0.18*b)) for v in base)+(255,)
                put(x,y,c)
    # Highlight at upper-left, thin inner circle and two small decorative dots.
    for y in range(20,77):
        for x in range(20,77):
            d=(x-48)**2+(y-47)**2
            if 25*25<d<27*27: put(x,y,(248,250,255,180))
    label=SYMBOL.get(name,(re.sub('[^A-Za-z0-9]','',name).upper()[:2] or '?'))
    scale=6 if len(label)<=2 else 4
    total=(len(label)*6-1)*scale
    ox=(96-total)//2;oy=26
    for i,ch in enumerate(label):
        for yy,line in enumerate(FONT.get(ch,FONT['?'])):
            for xx in range(5):
                if line&(1<<(4-xx)):
                    for py in range(scale):
                        for px in range(scale): put(ox+i*6*scale+xx*scale+px,oy+yy*scale+py,(255,255,255,255))
    rows=[pix[y*W*4:(y+1)*W*4] for y in range(H)]
    return png_encode(rows,W,H)

def all_names(root):
    names=set(EXTRA)
    for p in (root/'innehall'/'data'/'kort').glob('*.toml'):
        s=p.read_text(encoding='utf-8',errors='replace')
        names.update(re.findall(r'\bikon\s*=\s*"([a-zA-Z0-9_-]+)"',s))
        names.update(re.findall(r'\bikon:([a-zA-Z0-9_-]+)',s))
        # Variationsmallar använder ibland ikon:{form}, där form finns som en sträng i listan.
        # Ta därför med korta identifierare från TOML-korten (men aldrig långa instruktionstexter).
        names.update(re.findall(r'"([a-zåäö][a-zåäö0-9_-]{1,23})"', s))
    for p in [root/'src'/'Fonster.cpp',root/'innehall'/'matte'/'ikoner.py']:
        if not p.exists():continue
        s=p.read_text(encoding='utf-8',errors='replace')
        names.update(re.findall(r'ikonbild\("([a-zA-Z0-9_-]+)"',s))
        names.update(re.findall(r'"([a-zA-Z0-9_-]+)"\s*:\s*"([a-zA-Z0-9_-]+)"',s)[i][1] for i in range(len(re.findall(r'"([a-zA-Z0-9_-]+)"\s*:\s*"([a-zA-Z0-9_-]+)"',s))))
    return {x for x in names if re.fullmatch('[A-Za-z0-9_-]{1,55}',x)}

def main():
    if len(sys.argv) != 3:
        raise SystemExit("Användning: python3 generera-saknade-ikoner.py KÄLLMAPP UTMAPP")
    root = Path(sys.argv[1]).resolve()
    dst = Path(sys.argv[2]).resolve()  # alltid byggmappen – skriv aldrig in i källkodsträdet
    if not (root/'src'/'Fonster.cpp').is_file():raise SystemExit('Saknar Matte-programmets källkod')
    dst.mkdir(parents=True,exist_ok=True)
    new=0
    for name in sorted(all_names(root)):
        f=dst/(name+'.png')
        if not f.exists():f.write_bytes(draw(name));new+=1
    print('Reservikoner skapade:',new,'(' ,len(list(dst.glob('*.png'))),'totalt). Originalikoner bevaras.')
if __name__=='__main__': main()
