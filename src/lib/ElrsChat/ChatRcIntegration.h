// SPDX-License-Identifier: GPL-3.0-or-later
// ELRSChat modifications: 2026-09-19. See NOTICE.md in the repository root.
// Included once, late in tx_main.cpp, to use the upstream RC state directly.
// All calls run on the loop/SPI interrupt core, outside device callbacks.
#include "deferred.h"
volatile bool chatRcPaused = false;
bool chatHandsetPresent = false;
bool chatHandsetNeedsModel = false;
static bool chatSavedPrimary, chatSavedDual;
static PowerLevels_e chatSavedPower;

static chat::Result chatRcEntryCheck()
{
  if (isArmed) return chat::Result::Armed;
  if (!chatHandsetPresent || firmwareOptions.is_airport || InBindingMode
      || connectionState >= MODE_STATES || connectionState == awaitingModelId
      || commitInProgress || ModelUpdatePending || config.IsModified()
      || !handset->GetRCdataLastRecv()
      || micros() - handset->GetRCdataLastRecv() > 1000000UL)
    return chat::Result::RcBusy;
  return chat::Result::None;
}

chat::Result chatRcSuspend()
{
  auto result = chatRcEntryCheck();
  if (result != chat::Result::None) return result;
  static const device_t *const keep[] = {
    &Handset_device, &LED_device, &RGB_device,
#if defined(PLATFORM_ESP32) && !defined(PLATFORM_ESP32_C3)
    &Thermal_device,
#endif
  };
  // Wait out any in-flight core 0 RF/config callback, then prevent new ones.
  if (!devicesPauseExcept(keep, ARRAY_SIZE(keep))) return chat::Result::RcBusy;
  result = chatRcEntryCheck();
  if (result == chat::Result::None && hasDeferredFunction()) result = chat::Result::RcBusy;
  if (result != chat::Result::None) {
    devicesPauseExcept(nullptr, 0);
    return result;
  }
  noInterrupts();
  chatRcPaused = true;
  hwTimer::stop();
  Radio.SetTxIdleMode();
  busyTransmitting = false;
  chatSavedPrimary = FHSSusePrimaryFreqBand;
  chatSavedDual = FHSSuseDualBand;
  chatSavedPower = POWERMGNT::currPower();
  otaConnector.resetOutputQueue();
  linkStats.uplink_Link_quality = 0;
  linkStats.downlink_Link_quality = 0;
  setConnectionState(disconnected);
  interrupts();
  webserverPreventAutoStart = true;
  return chat::Result::None;
}

void chatRcRestore(bool resume)
{
  // An updater may already have ended the driver. Never revive its RF.
  if (connectionState < wifiUpdate) {
    noInterrupts();
    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;
    FHSSusePrimaryFreqBand = chatSavedPrimary;
    FHSSuseDualBand = chatSavedDual;
    busyTransmitting = false;
    TelemetryRcvPhase = ttrpTransmitting;
    NextPacketIsDataUl = false;
    syncTelemBoostState = stbIdle;
    RxWiFiReadyToSend = false;
    DataUlSender.ResetState();
    DataDlReceiver.ResetState();
    otaConnector.resetOutputQueue();
    LqTQly.reset();
    LastTLMpacketRecv_Ms = 0;
    RxDisconnected_Ms = 0;
    linkStats.uplink_Link_quality = 0;
    linkStats.downlink_Link_quality = 0;
    connectionHasModelMatch = false;
    // Bypass the stock same-rate cache: chat changed the actual radio config.
    config.SetRate(adjustPacketRateForBaud(config.GetRate()));
    ExpressLRS_currAirRate_Modparams = nullptr;
    SetRFLinkRate(config.GetRate());
    if (ModelUpdatePending || config.IsModified()) {
      POWERMGNT::setPower(PowerLevels_e(config.GetPower()));
      ResetPower();
    }
    else POWERMGNT::setPower(chatSavedPower);
    DynamicPower_Init();
    Radio.ApplyPendingPower();
    LbtEnableIfRequired();
    LbtCcaTimerStart();
    syncSpamCounter = syncSpamAmount;
    syncSpamCounterAfterRateChange = syncSpamAmountAfterRateChange;
    setConnectionState(!chatHandsetPresent ? noCrossfire :
        (chatHandsetNeedsModel ? awaitingModelId : disconnected));
    chatRcPaused = false;
    if (resume && chatHandsetPresent) hwTimer::resume();
    interrupts();
  } else {
    chatRcPaused = false;
  }
  devicesPauseExcept(nullptr, 0);
}
