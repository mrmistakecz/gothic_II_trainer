"""Read-only audit of the installed DAT and VDF map resources."""
import argparse, json, pathlib, struct

def read_dat(path):
    data=path.read_bytes(); p=0
    def u32():
        nonlocal p
        v=struct.unpack_from('<I',data,p)[0];p+=4;return v
    def line():
        nonlocal p
        end=data.index(b'\n',p);v=data[p:end].rstrip(b'\r').decode('cp1250',errors='replace');p=end+1;return v
    version=data[p];p+=1;count=u32();p+=count*4;symbols=[]
    for i in range(count):
        name=line() if u32() else ''
        vary=u32();prop=u32();ele=prop&4095;kind=(prop>>12)&15;flags=(prop>>16)&63
        p+=20;value=None
        if not flags&4:
            if kind in (1,2):
                value=list(struct.unpack_from('<'+'i'*ele,data,p));p+=ele*4
            elif kind==3:value=[line() for _ in range(ele)]
            elif kind in (4,5,6,7):value=u32()
        parent=u32()
        symbols.append(dict(name=name,type=kind,flags=flags,value=value))
    size=u32();return symbols,data[p:p+size]

def inspect(root):
    symbols,code=read_dat(root/'_work/Data/Scripts/_compiled/Gothic.dat')
    maps=[]
    for s in symbols:
        if s['type']!=5 or not s['name'].startswith('USE_MAP_'):continue
        pos=s['value'];instructions=[]
        for _ in range(500):
            op=code[pos];pos+=1;value=None
            if op in (61,62,64,65,67,75,76,80,245):
                value=struct.unpack_from('<i',code,pos)[0];pos+=4
                if op==245:pos+=1
            resolved=symbols[value]['name'] if op in (62,65,67,80,245) and 0<=value<len(symbols) else value
            if op==65 and 0<=value<len(symbols) and symbols[value]['type']==3:resolved=symbols[value]['value']
            instructions.append([op,resolved])
            if op==60:break
        maps.append(dict(function=s['name'],instructions=instructions))
    textures=[]
    for archive in (root/'Data').glob('*.vdf'):
        with archive.open('rb') as f:
            header=f.read(296)
            if len(header)!=296:continue
            count,_,_,_,catalog,_=struct.unpack_from('<6I',header,272)
            if count>1000000:continue
            f.seek(catalog)
            for _ in range(count):
                row=f.read(80)
                if len(row)<80:break
                name=row[:64].rstrip(b'\0 ').decode('ascii',errors='replace')
                if 'MAP_' in name.upper():textures.append(dict(archive=archive.name,name=name))
    return dict(game=str(root),map_functions=maps,map_resources=textures)

if __name__=='__main__':
    args=argparse.ArgumentParser();args.add_argument('game',type=pathlib.Path);args.add_argument('output',type=pathlib.Path);a=args.parse_args()
    result=inspect(a.game);a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(result,ensure_ascii=False,indent=2))
