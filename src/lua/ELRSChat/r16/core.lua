-- SPDX-License-Identifier: GPL-3.0-or-later
-- ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
-- ELRS Public Chat r16. GPL-3.0-or-later.
-- Private per-script state; no globals shared with other Lua tools.
local q = ...
local floor, sub = math.floor, string.sub
local function receive(d, now)
  if type(d)~="table" or #d<12 or #d>60 then return end
  for i=1,#d do
    if type(d[i])~="number" or d[i]<0 or d[i]>255 or d[i]%1~=0 then return end
  end
  if d[1]~=0xEF or d[2]~=0xEE
    or d[3]~=67 or d[4]~=72 or d[5]~=2 or q.token(d,7)~=q.client then return end
  if d[6]==0x81 and #d==44 then
    if d[25]>4 or d[26]>18 or d[27]<1 or d[27]>q.MAX or d[33]>7 or d[43]>1 or d[44]>2 then return end
    local newBoot=q.token(d,13)
    local newFrequency=q.le16(d,29)+q.le16(d,31)*65536.0
    local newId=q.id(d,17)
    -- Status polls continue at their original rate; identical polls do not
    -- repaint the retained color bitmap or invalidate message layout caches.
    q.redraw=q.redraw or q.boot~=newBoot or q.owner~=(d[43]==1) or q.mode~=d[44]
      or q.state~=d[25] or q.result~=d[26] or q.maxText~=d[27] or q.profile~=d[28]
      or q.power~=d[33] or q.dropped~=q.le16(d,39) or q.frequency~=newFrequency or q.ownId~=newId
      or (d[43]==1 and q.le16(d,41)>q.highWater)
    if q.boot and newBoot~=q.boot then
      q.failTx("UNKNOWN / REBOOT"); q.incoming=nil; q.releaseId=nil; q.releasedId=nil
      q.highWater=0
      q.needEnter=false; q.enterSent=false; q.view="entry"; q.show("MODULE REBOOTED")
    end
    q.boot=newBoot; q.last=now; q.owner=d[43]==1
    q.mode=d[44]
    q.state=d[25]; q.result=d[26]; q.maxText=math.min(q.MAX,d[27]); q.profile=d[28]
    -- Hz is display data only; configured settings are compared byte for byte.
    q.frequency=newFrequency
    q.power=d[33]; q.ownId=newId; q.dropped=q.le16(d,39)
    q.profileMatches=q.entryProfile~=nil and q.power==q.entryProfile[5]
    if q.profileMatches then
      for i=1,4 do if d[28+i]~=q.expectedHz[i] then q.profileMatches=false end end
    end
    if q.owner then
      q.highWater=math.max(q.highWater,q.le16(d,41))
    end
    if q.needEnter and q.enterSent and (q.le16(d,11)==1 or (q.owner and q.mode==1)) then
      q.needEnter=false; q.enterSent=false; q.redraw=true
      if q.owner and q.mode==1 and q.profileMatches then q.view="home"
      elseif q.owner and q.mode==1 then
        -- An adapter must not silently clamp or ignore the requested settings.
        q.show("RF SETTING MISMATCH"); q.pendingExit=now
      else q.show(not q.owner and q.mode==1 and "SESSION BUSY" or (q.results[q.result] or "ENTRY REFUSED")) end
    end
    if q.pendingExit and q.exitSent and q.le16(d,11)==q.exitRequest and q.mode==0 then
      q.exitConfirmed=true
    end
    if (q.mode~=1 or not q.owner) and q.view~="entry" and not q.pendingExit then
      q.failTx("CHAT ENDED"); q.view="entry"
      q.show(q.mode==0 and "RC MODE RESTORED" or "SESSION UNAVAILABLE")
    end
    if q.tx and q.le16(d,34)==q.tx.req then
      if q.tx.phase=="waitBegin" and q.result==1 and q.state==2 then q.tx.phase="data" end
      if q.result>=4 then q.failTx(q.results[q.result] or "ERROR") end
    end
    q.rxHead=q.le16(d,36)
    if q.rxHead~=q.releasedId then q.releasedId=nil end
    if q.incoming and (not q.owner or q.mode~=1 or q.rxHead~=q.incoming.id) then q.incoming=nil end
    if q.releaseId and (not q.owner or q.mode~=1 or q.rxHead~=q.releaseId) then q.releaseId=nil end
    if q.owner and q.mode==1 and q.rxHead~=0 and not q.incoming and not q.releaseId then
      if q.rxHead==q.releasedId then q.releaseId=q.rxHead
      else q.incoming={id=q.rxHead,bytes={},next=0,asked=now-30} end
    end
  elseif d[6]==0x87 and q.incoming and q.le16(d,11)==q.incoming.id and #d>=15 then
    local off, count=d[13],#d-14
    if d[14]~=58 or off~=q.incoming.next or count~=math.min(q.CHUNK,58-off) then return end
    q.last=now -- A validated record reply also proves the local module is alive.
    for i=1,count do q.incoming.bytes[off+i]=d[14+i] end
    q.incoming.next=off+count; q.incoming.asked=now-30
    if q.incoming.next==58 then
      local b=q.incoming.bytes
      if b[1]==69 and b[2]==67 and b[3]==1 and b[4]==q.profile and b[21]==0 and b[22]>=1 and b[22]<=q.MAX then
        -- RF CRC, UTF-8 and duplicate validation are done by the module engine.
        local key=""; for i=5,20 do key=key..string.char(b[i]) end
        if key~=q.lastRxKey then
          q.addMessage({direction="Rx",sender=q.id(b,5),text=q.visibleText(b,23,b[22])})
          q.lastRxKey=key
        end
      end
      q.releaseId=q.incoming.id; q.incoming=nil
    end
  end
end
local function pump(now)
  if not crossfireTelemetryPop or not crossfireTelemetryPush then return end
  for _=1,6 do
    local command,data=crossfireTelemetryPop()
    if not command then break end
    local response=q.transportPop(command,data,now)
    if response then receive(response,now) end
  end
  if q.tx and ((q.last and q.elapsed(now,q.last)>=150) or q.elapsed(now,q.tx.started)>650) then
    q.failTx(q.tx.phase=="waitDone" and "UNKNOWN" or "LOCAL TIMEOUT")
    q.needEnter=false
  end
  if q.transportTick(now) then return 0 end
  if q.pendingExit then
    if q.exitConfirmed then return 2 end
    if not q.exitSent or q.elapsed(now,q.exitSent)>=25 then
      if q.push(q.EXIT,q.exitRequest,nil,now) then q.exitSent=now end
    end
    return 0
  end
  if q.needEnter and q.enterSent and q.elapsed(now,q.lastEnter)>=150 then
    q.needEnter=false; q.show("ENTRY UNCONFIRMED")
  end
  -- A mode-changing Enter is issued once per explicit key press. Poll can
  -- recover a lost response; no delayed retry may suspend RC after a reboot.
  if q.needEnter and not q.enterSent then
    if q.push(q.ENTER,1,q.entryProfile,now) then q.lastEnter=now; q.enterSent=true end
  elseif q.due(now,q.nextPoll) and (not q.last or q.elapsed(now,q.last)>=75) then
    -- Keep status fresh even when slow Lua callbacks always have READ work.
    if q.push(q.POLL,0,nil,now) then q.nextPoll=(now+25)%16777216 end
  elseif q.tx and q.tx.phase=="begin" then
    if q.push(q.BEGIN,q.tx.req,{#q.tx.text},now) then q.tx.phase="waitBegin" end
  elseif q.tx and q.tx.phase=="data" then
    local body={q.tx.offset}
    for i=q.tx.offset+1,math.min(#q.tx.text,q.tx.offset+q.CHUNK) do body[#body+1]=string.byte(q.tx.text,i) end
    if q.push(q.DATA,q.tx.req,body,now) then
      q.tx.offset=q.tx.offset+#body-1
      if q.tx.offset==#q.tx.text then q.tx.phase="commit" end
    end
  elseif q.tx and q.tx.phase=="commit" then
    if q.push(q.COMMIT,q.tx.req,nil,now) then q.tx.phase="waitDone" end
  elseif q.releaseId then
    if q.push(q.RELEASE,q.releaseId,nil,now) then q.releasedId=q.releaseId; q.releaseId=nil end
  elseif q.incoming and q.elapsed(now,q.incoming.asked)>=30 then
    if q.push(q.READ,q.incoming.id,{q.incoming.next},now) then q.incoming.asked=now end
  elseif q.due(now,q.nextPoll) then
    if q.push(q.POLL,0,nil,now) then q.nextPoll=(now+25)%16777216 end
  end
  return 0
end
local function eventIs(event, constant) return constant~=nil and event==constant end
function q.run(event)
  local now=q.clock()
  local done=pump(now)
  if done==2 then return 2 end
  -- Most callbacks contain no key event. Timers and transport above still run.
  if not event or event==0 then q.draw(now); return 0 end
  q.redraw=true
  local nextKey=eventIs(event,EVT_VIRTUAL_NEXT) or eventIs(event,EVT_VIRTUAL_NEXT_REPT)
  local prevKey=eventIs(event,EVT_VIRTUAL_PREV) or eventIs(event,EVT_VIRTUAL_PREV_REPT)
  local enterKey=eventIs(event,EVT_VIRTUAL_ENTER)
  local longEnterKey=eventIs(event,EVT_VIRTUAL_ENTER_LONG)
  local exitKey=eventIs(event,EVT_VIRTUAL_EXIT)
  -- ENTER is a release event. Consume the release following a long press
  -- ourselves: killEvents() cannot mask ENTER on all EdgeTX builds.
  -- A fresh FIRST (where delivered) also recovers if the old release was lost.
  if eventIs(event,EVT_ENTER_FIRST) then q.suppressEnterRelease=false end
  if q.suppressEnterRelease then
    if enterKey then q.suppressEnterRelease=false; enterKey=false end
    longEnterKey=false
  end
  local delta=nextKey and 1 or (prevKey and -1 or 0)
  if q.pendingExit then
    if exitKey and q.elapsed(now,q.pendingExit)>=800 then return 2 end
    q.draw(now); return 0
  end
  if q.view=="entry" then
    if enterKey and not q.needEnter then
      if q.configError then q.show(q.configError)
      elseif q.connected(now) then q.needEnter=true; q.enterSent=false
      else
        q.show(q.linkHint or "NO MODULE REPLY")
        if q.scanDone then q.scanDone=nil; q.discoverAt=now end
      end
    end
    if exitKey then
      if not q.enterSent and not q.owner then return 2 end
      q.needEnter=false; q.pendingExit=now
    end
  elseif q.view=="home" then
    q.scroll=math.max(1,math.min(q.lastScroll(),q.scroll+delta))
    if enterKey then q.view="menu"; q.choice=1 end
    if exitKey then q.pendingExit=now end
  elseif q.view=="menu" or q.view=="presets" then
    local list=q.view=="menu" and q.menu or q.presets
    q.choice=(q.choice-1+delta)%#list+1
    if exitKey then q.view="home" end
    if enterKey then
      if q.view=="presets" then q.draft=sub(q.presets[q.choice],1,q.maxText); q.view="confirm"
      elseif q.choice==1 then q.view="presets"; q.choice=1
      elseif q.choice==2 then q.view="compose"; q.picker=2
      elseif q.choice==3 then q.view="info"
      elseif q.choice==4 then
        if not q.tx then q.view="entry" end
      elseif q.choice==5 then q.pendingExit=now end
    end
  elseif q.view=="compose" then
    q.picker=(q.picker-1+delta)%(#q.chars+3)+1
    if longEnterKey then
      q.suppressEnterRelease=true
      q.sendDraft(now)
    elseif enterKey then
      if q.picker<=#q.chars then
        if #q.draft<q.maxText then q.draft=q.draft..sub(q.chars,q.picker,q.picker) else q.show("MESSAGE FULL") end
      elseif q.picker==#q.chars+1 then q.draft=sub(q.draft,1,-2)
      elseif q.picker==#q.chars+2 then q.view="confirm"
      else q.view="home" end
    end
    if exitKey then q.view="home" end
  elseif q.view=="confirm" then
    if exitKey then q.view="compose"; q.picker=#q.chars+2 end
    if enterKey then q.sendDraft(now) end
  elseif exitKey then q.view="home" end
  q.draw(now)
  return 0
end
