"""Print the Unified metadata following an ESP application image."""
import json
import struct
import sys
from pathlib import Path

def metadata(path):
    data=Path(path).read_bytes()
    if data[0]!=0xe9: raise ValueError('Not an ESP application image')
    count=data[1]; esp8266=count==2
    pos=24
    if esp8266:
        count=data[0x1001]; pos=0x1008
    for _ in range(count):
        size=struct.unpack_from('<I',data,pos+4)[0]; pos+=8+size
    end=((pos+16)&~15)+(0 if esp8266 else 32)
    result={'bytes':len(data),'application_end':end,'metadata_bytes':len(data)-end}
    if len(data)>=end+2704:
        result['product']=data[end:end+128].split(b'\0')[0].decode()
        for name,offset,size in [('options',144,512),('hardware',656,2048)]:
            raw=data[end+offset:end+offset+size].split(b'\0')[0]
            result[name]=json.loads(raw) if raw else None
    return result

if __name__=='__main__':
    print(json.dumps(metadata(sys.argv[1]),indent=2))
