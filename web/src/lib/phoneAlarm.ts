// Acknowledge a ringing Bluetooth phone alarm (spec 009), from the dashboard
// (specs/025 FR-017). False if the device couldn't be reached.
export const acknowledgePhoneAlarm = async (): Promise<boolean> => {
  try {
    return (await fetch('/api/ble/ack', { method: 'POST' })).ok;
  } catch {
    return false;
  }
};
