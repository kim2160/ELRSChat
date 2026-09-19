-- SPDX-License-Identifier: GPL-3.0-or-later
-- ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
-- ELRS Public Chat r16. GPL-3.0-or-later.
-- Private per-script state; no globals shared with other Lua tools.
local q = ...
local floor, sub = math.floor, string.sub
local function text(y,s,flags,x)
  local ink=q.color and (y==0 and q.headerText or (y==56 and q.footerText)) or q.textColor
  lcd.drawText(x or q.textInset,q.rowY[floor(y/8)],s,(flags or 0)+q.textFont+ink)
end
local function header(title)
  -- 128px needs all 21 cells to keep PUBLIC CHAT READY and the four-digit ID.
  text(0,title,q.color and 0 or (INVERS or 0),not q.color and q.screenW<=128 and 0 or nil)
end
local function footer(s,flags)
  lcd.drawText(q.textInset,q.footerY,s,(flags or 0)+q.textFont+(q.color and q.footerText or q.textColor))
end
local function drawDraft()
  if q.draftCache~=q.draft then q.draftLines=q.wrap("",q.draft); q.draftCache=q.draft end
  for i=1,#q.draftLines do text(8+i*8,q.draftLines[i]) end
end
function q.draw(now)
  local linked=q.connected(now) and true or false
  local alert=not q.pendingExit and q.notice and not q.due(now,q.noticeUntil)
  local exitWarning=q.pendingExit and q.elapsed(now,q.pendingExit)>=800
  -- EdgeTX color standalone tools own a persistent bitmap. Keep transport
  -- running every callback; repaint on changes/input, plus a 1-second refresh.
  -- Monochrome may clear its shared LCD buffer outside Lua, so always redraw it.
  if q.color and not q.redraw and linked==q.paintedLink and alert==q.paintedAlert
      and exitWarning==q.paintedExit and q.elapsed(now,q.lastDraw)<100 then return end
  q.redraw=false; q.lastDraw=now; q.paintedLink=linked; q.paintedAlert=alert; q.paintedExit=exitWarning
  if q.color then
    lcd.clear(q.background)
    lcd.drawFilledRectangle(0,0,q.screenW,q.textTop+q.textHeight+2,q.headerColor)
    lcd.drawFilledRectangle(0,q.footerY-2,q.screenW,q.screenH-q.footerY+2,q.footerColor)
  else lcd.clear() end
  local link=q.connected(now) and (q.mode==0 and "RC" or (q.owner and (q.states[q.state] or "?") or "BUSY")) or "NO LINK"
  if q.pendingExit then
    header("RETURNING TO RC")
    text(16,"WAIT FOR MODULE...")
    if q.elapsed(now,q.pendingExit)>=800 then
      text(32,"RC NOT CONFIRMED")
      text(40,"CHECK MODULE / LINK")
      text(56,"EXIT:CLOSE ANYWAY")
    end
  elseif q.view=="entry" then
    header("ENTER PUBLIC CHAT?")
    text(16,"RC CONTROL WILL STOP")
    text(24,q.configError or (q.needEnter and "ENTERING..." or
      (not linked and (q.linkHint or "FINDING MODULE") or (q.result==4 and "READY" or (q.results[q.result] or "READY")))))
    if q.configLabel then text(32,q.configLabel) end
    text(56,"ENT:CHAT  EXIT:CLOSE")
  elseif q.view=="home" then
    -- Position ID in pixels: spaces are proportional on color displays.
    local title="PUBLIC CHAT "..(not q.sendBlockReason(now) and "READY" or (link=="NO LINK" and "OFF" or link))
    header(title)
    if q.color then
      text(0,q.ownId,RIGHT or 0,q.idX)
    else
      -- Monochrome RIGHT measures proportional glyph widths even when
      -- FIXEDWIDTH is set, so place these four fixed 6px cells ourselves.
      text(0,q.ownId,INVERS or 0,q.idX)
    end
    if #q.logLines>0 then
      for i=0,q.logRows-1 do
        local line=q.logLines[q.scroll+i]
        if line then
          local y=q.logTop+i*q.logHeight
          if q.color and q.logShades[q.scroll+i] then
            -- Shade all received lines, including wrapped continuations.
            lcd.drawFilledRectangle(q.edge,y,q.screenW-2*q.edge,q.logHeight,q.stripeColor)
          end
          lcd.drawText(q.textInset,y+(q.color and 1 or 0),line,q.textFont+q.textColor)
        end
      end
    else
      text(16,"NO MESSAGES YET")
      if not q.connected(now) then text(32,"Enable ELRS / CRSF") end
    end
    if not alert then footer("ENT:MENU +/-:SCROLL") end
  elseif q.view=="menu" or q.view=="presets" then
    local list=q.view=="menu" and q.menu or q.presets
    header(q.view=="menu" and "CHAT MENU" or "PRESET MESSAGE")
    local first=math.max(1,q.choice-q.menuRows+1)
    for i=first,math.min(#list,first+q.menuRows-1) do
      lcd.drawText(q.textInset,q.rowY[1]+(i-first)*q.lineHeight,q.view=="presets" and q.presetLabels[i] or list[i],
        (i==q.choice and (INVERS or 0) or 0)+q.textFont+q.textColor)
    end
    text(56,"ENT:SELECT  EXIT:BACK")
  elseif q.view=="compose" then
    header("WRITE "..#q.draft.."/"..q.maxText)
    drawDraft()
    local pick
    if q.picker<=#q.chars then pick=q.picker==1 and "SPACE" or sub(q.chars,q.picker,q.picker)
    else pick=q.actions[q.picker-#q.chars] end
    text(40,"[ "..pick.." ]",INVERS or 0)
    if not alert then text(48,"HOLD ENT:SEND") end
    text(56,"+/-:PICK  ENT:USE")
  elseif q.view=="confirm" then
    header("SEND TO PUBLIC ROOM?")
    drawDraft()
    text(56,"ENT:SEND   EXIT:BACK")
  elseif q.view=="info" then
    header("MODULE INFO")
    text(8,link.." "..(q.result==4 and "READY" or (q.results[q.result] or "")))
    text(16,"ID "..q.ownId.." PROFILE "..q.profile)
    text(24,string.format("%.3f MHz",q.frequency/1000000))
    text(32,"POWER "..(q.powers[q.power+1] or 0).."mW")
    text(40,"DROP "..q.dropped)
    if not alert then text(48,"LUA R16") end
    text(56,"EXIT:BACK")
  end
  if alert then
    if q.view=="home" then footer(q.notice,INVERS or 0)
    else text(48,q.notice,INVERS or 0) end
  end
end
function q.initLayout()
  -- EdgeTX provides the native full-screen dimensions; no radio-name table.
  q.screenW=LCD_W or 128; q.screenH=LCD_H or 64
  q.color = lcd.RGB ~= nil and q.screenH >= 128
  q.edge=q.color and math.max(4,math.min(12,floor(math.min(q.screenW,q.screenH)/40))) or 0
  q.textInset=q.color and q.edge+2 or floor((q.screenW%6)/2)
  q.textTop=q.color and q.edge+2 or floor((q.screenH%8)/2)
  q.contentWidth=q.screenW-2*q.textInset
  local rowLimit=floor((q.screenH-2*q.textTop)/8)
  q.textHeight=q.color and rowLimit-4 or 8
  q.textFont=q.color and 0 or (FIXEDWIDTH or 0)
  if q.color and lcd.sizeText then
    -- W is not necessarily the widest glyph. Check the full printable wire
    -- alphabet and remeasure each smaller font, including inverse margins.
    for _,font in ipairs({0,SMLSIZE or 0,TINSIZE or SMLSIZE or 0}) do
      local widest, tallest=0,0
      for c=32,126 do
        local w,h=lcd.sizeText(string.char(c),font)
        widest=math.max(widest,w); tallest=math.max(tallest,h)
      end
      q.textFont=font; q.textHeight=tallest
      if widest*21+4<=q.contentWidth and tallest+4<=rowLimit then break end
    end
  elseif q.color then
    q.textFont=SMLSIZE or 0
  end
  q.textColor=q.color and lcd.RGB(0,0,0) or 0
  q.background=q.color and lcd.RGB(255,255,255) or 0
  q.stripeColor=q.color and lcd.RGB(208,208,208) or 0
  q.headerColor=q.color and lcd.RGB(50,66,82) or 0
  q.headerText=q.color and lcd.RGB(248,250,252) or 0
  q.footerColor=q.color and lcd.RGB(229,235,241) or 0
  q.footerText=q.color and lcd.RGB(61,76,91) or 0
  -- Fit only menu labels; confirmation and RF retain the full preset text.
  -- Cache once at init, so scrolling never measures or allocates labels.
  q.presetLabels={}
  for i=1,#q.presets do
    local label=q.presets[i]
    if not q.fits(label) then
      local n=#label
      repeat n=n-1; label=sub(q.presets[i],1,n).."..." until n==0 or q.fits(label)
    end
    q.presetLabels[i]=label
  end
  q.footerY=q.screenH-q.textTop-q.textHeight
  q.lineHeight=q.color and math.min(q.textHeight+6,floor((q.footerY-q.textTop)/7)) or 8
  q.rowY={}
  for i=0,5 do q.rowY[i]=q.textTop+i*q.lineHeight end
  q.rowY[6]=q.footerY-q.lineHeight; q.rowY[7]=q.footerY
  q.menuRows=math.min(#q.presets,math.max(1,
    floor((q.rowY[6]-(q.color and 4 or 0)-q.rowY[1]-q.textHeight)/q.lineHeight)+1))
  q.idX=q.color and q.screenW-q.textInset or q.screenW-24-(q.screenW>128 and q.textInset or 0)
  q.logTop=q.color and q.textTop+q.textHeight+4 or q.rowY[1]
  q.logHeight=q.textHeight+(q.color and 2 or 0)
  q.logRows=math.max(1,floor((q.footerY-(q.color and 4 or 0)-q.logTop)/q.logHeight))
end
