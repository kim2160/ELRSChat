-- SPDX-License-Identifier: GPL-3.0-or-later
-- ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
-- ELRS Public Chat r16. GPL-3.0-or-later.
-- Private per-script state; no globals shared with other Lua tools.
local q = ...
local floor, sub = math.floor, string.sub
local function hint(message)
  if q.linkHint~=message then q.linkHint=message; q.redraw=true end
end
local function nextField(now)
  q.scanField=q.scanField and q.scanField>1 and q.scanField-1 or nil
  if not q.scanField then
    hint(q.scanLost and "CHAT API TIMEOUT" or "CHAT API NOT FOUND")
    q.scanDone=true -- Keep the result visible until an explicit retry.
  end
end
-- One dynamically registered STRING mailbox; no private CRSF frame number.

local alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
local function base64(p,first)
  local out=""
  for i=first,#p,3 do
    local v=p[i]*65536+(p[i+1] or 0)*256+(p[i+2] or 0)
    local a,b=floor(v/262144)%64,floor(v/4096)%64
    local c,d=floor(v/64)%64,v%64
    out=out..sub(alphabet,a+1,a+1)..sub(alphabet,b+1,b+1)
      ..(i+1<=#p and sub(alphabet,c+1,c+1) or "=")
      ..(i+2<=#p and sub(alphabet,d+1,d+1) or "=")
  end
  return out
end
local function unbase64(s)
  if #s==0 or #s>56 or #s%4~=0 then return end
  local out={0xEF,0xEE}
  for i=1,#s,4 do
    local v,pad=0,0
    for j=0,3 do
      local c=sub(s,i+j,i+j)
      local b=string.byte(c)
      local n
      if b>=65 and b<=90 then n=b-65
      elseif b>=97 and b<=122 then n=b-71
      elseif b>=48 and b<=57 then n=b+4
      elseif b==43 then n=62 elseif b==47 then n=63 end
      if n and pad==0 then v=v*64+n
      elseif c=="=" and i+3==#s and j>=2 then v=v*64; pad=pad+1
      else return end
    end
    if (pad==1 and v%256~=0) or (pad==2 and v%65536~=0) then return end
    out[#out+1]=floor(v/65536)
    if pad<2 then out[#out+1]=floor(v/256)%256 end
    if pad==0 then out[#out+1]=v%256 end
  end
  return out
end
local function wirePush(frame,data,now)
  if not q.due(now,q.nextWire) or not crossfireTelemetryPush then return false end
  if crossfireTelemetryPush(frame,data) then q.nextWire=(now+3)%16777216; return true end
  return false
end
function q.push(op, req, body, now)
  if not q.mailboxField or q.mailRead then return false end
  local encoded=base64(q.packet(op,req,body),3)
  local d={0xEE,0xEF,q.mailboxField}
  for i=1,#encoded do d[#d+1]=string.byte(encoded,i) end
  d[#d+1]=0
  if wirePush(0x2D,d,now) then
    q.mailRead={chunk=0,bytes={},next=now,since=now,waiting=false}
    return true
  end
  return false
end
function q.transportPop(frame,d,now)
  if type(d)~="table" or #d<2 or #d>60 then return end
  for i=1,#d do
    if type(d[i])~="number" or d[i]<0 or d[i]>255 or d[i]%1~=0 then return end
  end
  if frame==0x29 and d[2]==0xEE and (d[1]==0xEF or d[1]==0xEA or d[1]==0) then
    if q.mailboxField or q.scanField or q.mailRead or q.scanDone then return end
    -- Read the field count relative to the terminated device name, as the
    -- official ELRS Lua does. Do not infer it from trailing packet bytes.
    local offset=3
    while offset<=#d and d[offset]~=0 do offset=offset+1 end
    if offset+14>#d then return end
    local count=d[offset+13]
    q.scanLost=false
    if count>0 then q.scanField=count; hint("SCANNING CHAT API")
    else hint("CHAT API NOT FOUND"); q.scanDone=true end
    return
  end
  if d[1]~=0xEF or d[2]~=0xEE then return end
  if frame~=0x2B or not q.mailRead or not q.mailRead.waiting or #d<5
    or d[3]~=(q.mailboxField or q.scanField) then return end
  local r=q.mailRead
  if r.remaining and d[4]~=r.remaining-1 then return end
  if d[4]>20 or #r.bytes+#d-4>128 then
    q.mailRead=nil
    if not q.mailboxField then q.scanLost=true; nextField(now) end
    return
  end
  for i=5,#d do r.bytes[#r.bytes+1]=d[i] end
  if not q.mailboxField then
    -- Other ELRS fields (e.g. packet-rate options) can exceed our mailbox
    -- buffer. Reject an unrelated type/name immediately, before requesting
    -- its remaining chunks. Keep partial matching headers for small frames.
    local bytes=r.bytes
    local unrelated=#bytes>=2 and bytes[2]%128~=10
    local expected="ELRS Chat\0"
    for i=3,math.min(#bytes,12) do
      if bytes[i]~=string.byte(expected,i-2) then unrelated=true; break end
    end
    if unrelated then q.mailRead=nil; nextField(now); return end
  end
  if d[4]>0 then
    r.chunk=r.chunk+1; r.remaining=d[4]; r.waiting=false; r.next=now
    return
  end
  q.mailRead=nil
  local bytes=r.bytes
  if #bytes<4 then
    if not q.mailboxField then q.scanLost=true; nextField(now) end
    return
  end
  local name,lastByte="",nil
  for i=3,#bytes do
    if bytes[i]==0 then lastByte=i; break end
    name=name..string.char(bytes[i])
  end
  if name~="ELRS Chat" or bytes[2]%128~=10 or not lastByte then
    if not q.mailboxField then nextField(now) end
    return
  end
  if not q.mailboxField then
    q.mailboxField=q.scanField; q.scanField=nil; hint("WAIT CHAT STATUS")
  end
  local value=""
  for i=lastByte+1,#bytes do
    if bytes[i]==0 then return unbase64(value) end
    value=value..string.char(bytes[i])
  end
end
function q.transportTick(now)
  if q.mailRead then
    if q.elapsed(now,q.mailRead.since)>200 then
      q.mailRead=nil
      if not q.mailboxField then q.scanLost=true; nextField(now)
      else hint("CHAT STATUS TIMEOUT") end
      return true
    end
    if q.due(now,q.mailRead.next) then
      -- Retry the complete snapshot, never mix a stale chunk with a new one.
      if q.mailRead.waiting then q.mailRead.chunk=0; q.mailRead.bytes={}; q.mailRead.remaining=nil end
      if wirePush(0x2C,{0xEE,0xEF,q.mailboxField or q.scanField,q.mailRead.chunk},now) then
        q.mailRead.waiting=true; q.mailRead.next=(now+50)%16777216
      end
    end
    return true
  end
  if not q.mailboxField then
    if q.scanDone then return true end
    if q.scanField then
      q.mailRead={chunk=0,bytes={},next=now,since=now,waiting=false}
      return q.transportTick(now)
    elseif q.due(now,q.discoverAt) and wirePush(0x28,{0xEE,0xEF},now) then
      q.discoverAt=(now+100)%16777216
      if q.linkHint=="FINDING MODULE" then hint("NO MODULE REPLY") end
    end
    return true
  end
  return false
end
