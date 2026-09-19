-- ELRS Public Chat r16. GPL-3.0-or-later.
-- Private per-script state; no globals shared with other Lua tools.
local q = ...
local floor, sub = math.floor, string.sub
q.MAX, q.CHUNK = 32, 16
q.HISTORY_LIMIT = 50
q.POLL, q.ENTER, q.EXIT, q.BEGIN, q.DATA, q.COMMIT, q.READ, q.RELEASE = 1,2,3,4,5,6,7,8
q.states = {[0]="IDLE", "RX", "EDIT", "WAIT", "TX"}
q.results = {[0]="READY", "ASSEMBLING", "QUEUED", "SENDING", "SENT", "CANCELLED",
  "INVALID", "INCOMPLETE", "CHANNEL BUSY", "EXPIRED", "RADIO ERROR", "UNKNOWN",
  "WAIT 1 SECOND", "NO RF PROFILE", "UNSUPPORTED", "INACTIVE", "STALE", "DISARM FIRST", "RC BUSY - RETRY"}
q.presets = {"ALL OK", "HELP NEEDED", "WHERE ARE YOU?", "SEND YOUR LOCATION",
  "MESSAGE RECEIVED", "WAITING HERE", "ON MY WAY", "PLEASE REPEAT",
  "I AM INJURED", "CANNOT MOVE", "NEED MEDICAL HELP", "NEED EVACUATION",
  "CALL EMERGENCY SERVICES", "DANGER HERE", "MOVE TO A SAFE PLACE",
  "STAY WHERE YOU ARE", "RETURN TO BASE", "NEED WATER", "NEED SHELTER", "LOW BATTERY"}
q.menu = {"Preset message", "Write message", "Module info", "Reconnect", "Exit chat"}
q.chars = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?!:/+-"
q.actions = {"DELETE", "SEND", "CANCEL"}
q.powers = {10,25,50,100,250,500,1000,2000}
-- Keep UI timers in a small exact integer ring, including across signed
-- getTime() rollover on Lua 5.3. All live deadlines are at most eight seconds.
function q.clock() return getTime()%16777216 end
function q.elapsed(now, since) return (now-since)%16777216 end
function q.due(now, deadline) return q.elapsed(now,deadline)<8388608 end

function q.le16(t, i)
  return (t[i] or 0)+(t[i+1] or 0)*256
end
function q.token(t, i)
  -- Opaque uint32 IDs must remain exact: neither signed int32 nor float32
  -- can represent every wire ID as a positive number.
  return string.char(t[i],t[i+1],t[i+2],t[i+3])
end
local function append(t, n, count)
  for _=1,count do t[#t+1]=n%256; n=floor(n/256) end
end
function q.prepareProfile()
  q.entryProfile=nil; q.expectedHz=nil; q.configError=nil; q.configLabel=nil; q.profileMatches=false
  local khz=q.CHAT_FREQUENCY_KHZ
  if type(khz)~="number" or not ((khz>=2401000 and khz<=2482000) or (khz>=863000 and khz<=928000))
      or khz%1~=0 then q.configError="BAD CHAT FREQUENCY"; return end
  local powerIndex
  for i=1,#q.powers do if q.CHAT_POWER_MW==q.powers[i] then powerIndex=i-1 end end
  if not powerIndex then q.configError="BAD CHAT POWER"; return end
  q.entryProfile={}; append(q.entryProfile,khz,4); q.entryProfile[5]=powerIndex
  -- Multiply byte by byte, never constructing >2^31 integers or rounded Hz.
  q.expectedHz={}; local carry=0
  for i=1,4 do
    local n=q.entryProfile[i]*1000+carry
    q.expectedHz[i]=n%256; carry=floor(n/256)
  end
  q.configLabel=string.format("%d.%03dMHz %dmW",floor(khz/1000),khz%1000,q.CHAT_POWER_MW)
end
function q.id(t, offset)
  -- Mix all six MAC bytes so devices sharing a manufacturer prefix differ.
  local h=(t[offset] or 0)*256+(t[offset+1] or 0)
    +(t[offset+2] or 0)*257+(t[offset+3] or 0)*17
    +(t[offset+4] or 0)*31+(t[offset+5] or 0)*127
  return string.format("%04X",h%65536)
end
function q.show(text)
  q.notice=text; q.noticeUntil=(q.clock()+200)%16777216; q.redraw=true
end
function q.packet(op, request, body)
  local d={0xEE,0xEF,67,72,2,op}
  for i=1,4 do d[#d+1]=string.byte(q.client,i) end
  append(d,request or 0,2)
  if body then for _,v in ipairs(body) do d[#d+1]=v end end
  return d
end
local function prefix(m)
  if m.direction=="Rx" then return "Rx("..m.sender.."):" end
  return m.failed and "Tx!:" or "Tx:"
end
function q.fits(s)
  if q.color and lcd.sizeText then
    local width=lcd.sizeText(s,q.textFont)
    return width<=q.contentWidth
  end
  return #s<=(q.color and 21 or floor(q.contentWidth/6))
end
function q.wrap(lead,body)
  local lines={}
  repeat
    local count=#body
    if not q.fits(lead..body) then
      -- Binary search bounds measurements even for the widest 32-byte text.
      local low,high=0,count-1
      while low<high do
        local mid=floor((low+high+1)/2)
        if q.fits(lead..sub(body,1,mid)) then low=mid else high=mid-1 end
      end
      count=low
    end
    -- A prefix and at least one character fit the selected baseline font.
    count=math.max(1,count)
    lines[#lines+1]=lead..sub(body,1,count)
    body=sub(body,count+1); lead=" "
  until #body==0
  return lines
end
local function lineCount(m) return #m end
function q.lastScroll() return math.max(1,#q.logLines-q.logRows+1) end
local function rebuildLog()
  local oldCount,count=#q.logLines,0
  for i=1,#q.history do
    local m=q.history[i]
    for _,line in ipairs(m) do
      count=count+1; q.logLines[count]=line; q.logShades[count]=m.rx
    end
  end
  for i=count+1,oldCount do q.logLines[i]=nil; q.logShades[i]=nil end
end
function q.addMessage(m)
  local atEnd=q.scroll==q.lastScroll()
  -- Store cached display lines without a second copy of every raw message.
  -- The active send already retains its raw text for failure-prefix reflow.
  local lines=q.wrap(prefix(m),m.text)
  lines.rx=m.direction=="Rx"
  if #q.history==q.HISTORY_LIMIT then
    q.scroll=math.max(1,q.scroll-lineCount(q.history[1]))
    for i=1,q.HISTORY_LIMIT-1 do q.history[i]=q.history[i+1] end
    q.history[q.HISTORY_LIMIT]=nil
  end
  q.history[#q.history+1]=lines
  rebuildLog()
  q.scroll=atEnd and q.lastScroll() or math.min(q.scroll,q.lastScroll())
  q.redraw=true
  return lines
end
function q.failTx(label)
  if q.tx then
    if label~="SENT" then
      local atEnd=q.scroll==q.lastScroll()
      local rowStart=1
      for i=1,#q.history do
        if q.history[i]==q.tx.row then break end
        rowStart=rowStart+lineCount(q.history[i])
      end
      local oldCount=lineCount(q.tx.row)
      local lines=q.wrap("Tx!:",q.tx.text)
      for i=1,math.max(oldCount,#lines) do q.tx.row[i]=lines[i] end
      rebuildLog()
      -- An 18-character Tx gains a wrapped line when its prefix becomes Tx!:.
      -- Keep the same older/newer record at the top when reading below it.
      if q.scroll>=rowStart+oldCount then q.scroll=q.scroll+lineCount(q.tx.row)-oldCount end
      q.scroll=atEnd and q.lastScroll() or math.min(q.scroll,q.lastScroll())
      q.show(label)
    end
    q.tx=nil; q.redraw=true
  end
end
function q.connected(now) return q.last and q.elapsed(now,q.last)<150 end
function q.sendBlockReason(now)
  if not q.connected(now) then return "NO MODULE LINK" end
  if not q.owner then return "SESSION BUSY" end
  if q.mode~=1 then return "ENTER CHAT FIRST" end
  if not q.profileMatches then return "RF SETTING MISMATCH" end
  if q.tx then return "TX IN PROGRESS" end
  if q.state~=1 then return "RF STATE: "..(q.states[q.state] or "UNKNOWN") end
  if q.frequency<=0 then return "NO RF PROFILE" end
  if q.highWater>=65535 then return "ID LIMIT: REOPEN LUA" end
end
function q.sendDraft(now)
  if #q.draft==0 then q.show("EMPTY MESSAGE"); return end
  local reason=q.sendBlockReason(now)
  if reason then q.show(reason); return end
  local row=q.addMessage({direction="Tx",text=q.draft})
  q.scroll=q.lastScroll()
  q.highWater=q.highWater+1
  q.tx={req=q.highWater,text=q.draft,phase="begin",offset=0,started=now,row=row}
  q.view="home"; q.draft=""; q.notice=nil
end
function q.visibleText(t, first, count)
  local s=""
  -- The default monochrome font does not contain Hangul. Keep the wire bytes,
  -- replace non-ASCII code points with '?' for this baseline UI.
  for i=first,first+count-1 do
    local b=t[i]
    if b>=32 and b<=126 then s=s..string.char(b)
    elseif b>=192 then s=s.."?" end
  end
  return s
end
function q.initState()
  q.prepareProfile()
  q.draftCache=nil; q.draftLines=nil; q.redraw=true; q.lastDraw=q.clock()
  q.paintedLink=nil; q.paintedAlert=nil; q.paintedExit=nil
  -- Changes between script launches; not a secret or an authenticated identity.
  local t=getDateTime and getDateTime() or {}
  -- Build two bounded 16-bit words, without overflowing integer arithmetic
  -- or rounding a 32-bit nonce through a single-precision float.
  local ticks=getTime()
  local lo=(ticks%65536+(t.sec or 0)*997+(t.min or 0)*257)%65536
  local hi=(floor(ticks/65536)+(t.day or 0)*1440+(t.hour or 0)*60+(t.min or 0))%65535+1
  q.client=string.char(lo%256,floor(lo/256),hi%256,floor(hi/256))
  q.boot=nil; q.last=nil; q.nextPoll=q.clock(); q.nextWire=q.nextPoll; q.needEnter=false; q.lastEnter=0
  q.mailboxField=nil; q.scanField=nil; q.mailRead=nil; q.discoverAt=q.nextPoll
  q.linkHint="FINDING MODULE"; q.scanLost=false
  q.owner=false; q.state=0; q.result=0; q.maxText=q.MAX; q.profile=0; q.frequency=0; q.power=0
  q.highWater=0; q.ownId="----"; q.dropped=0; q.history={}; q.logLines={}; q.scroll=1
  q.logShades={}
  q.view="entry"; q.choice=1; q.picker=2; q.draft=""; q.tx=nil; q.incoming=nil
  q.mode=nil; q.exitSent=nil; q.exitRequest=65535; q.exitConfirmed=false; q.enterSent=false
  q.suppressEnterRelease=false
  q.releaseId=nil; q.releasedId=nil; q.pendingExit=nil; q.lastRxKey=nil; q.rxHead=0; q.notice=nil; q.noticeUntil=0
end
