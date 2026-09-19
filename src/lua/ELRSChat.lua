-- ELRS Public Chat r16 / EdgeTX. GPL-3.0-or-later.
-- Copy this file AND the ELRSChat folder to /SCRIPTS/TOOLS/.
-- Same r13-capable module firmware; no changes to RF or normal RC.
local CHAT_FREQUENCY_KHZ = 2440000 -- 2440.000 MHz; peers use the same channel.
local CHAT_POWER_MW = 25          -- Must be supported by module calibration.
local path="/SCRIPTS/TOOLS/ELRSChat/r16/"
local files={"common","transport","view","core"}
local app, problem

local function init()
  app=nil; problem=nil
  collectgarbage("collect")
  -- Compile small chunks separately, without retaining the source compiler's
  -- debug data. EdgeTX writes stripped .luac caches on the SD card. Loading all
  -- source in one pass can exceed a monochrome radio's heap before init runs.
  for i=1,#files do
    local chunk=loadScript(path..files[i]..".lua","bt")
    if not chunk then problem="COPY ELRSChat FOLDER"; return end
    chunk=nil; collectgarbage("collect")
  end
  app={CHAT_FREQUENCY_KHZ=CHAT_FREQUENCY_KHZ,CHAT_POWER_MW=CHAT_POWER_MW}
  for i=1,#files do
    local chunk=loadScript(path..files[i]..".lua","b")
    if not chunk then app=nil; problem="SD WRITE ERROR"; return end
    chunk(app)
    chunk=nil; collectgarbage("collect")
  end
  app.initState(); app.initLayout()
  app.initState=nil; app.initLayout=nil; app.prepareProfile=nil
  collectgarbage("collect")
end

local function run(event)
  if problem then
    lcd.clear()
    lcd.drawText(0,0,"ELRS CHAT R16",INVERS or 0)
    lcd.drawText(0,16,problem,0)
    lcd.drawText(0,32,"REOPEN AFTER COPYING",0)
    lcd.drawText(0,56,"EXIT:CLOSE",0)
    return event==EVT_VIRTUAL_EXIT and 2 or 0
  end
  return app.run(event)
end
return {init=init,run=run}
