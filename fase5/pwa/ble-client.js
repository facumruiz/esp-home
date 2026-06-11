/**
 * ble-client.js — ESP-HOME Web Bluetooth client
 *
 * Reemplaza toda la comunicación HTTP/WebSocket por BLE.
 * Expone la misma interfaz lógica que los endpoints REST anteriores.
 *
 * USO:
 *   const ble = new EspHomeBLE();
 *   await ble.connect();
 *   await ble.setLED(true);
 *   await ble.setStrip1({ s: true, r: 255, g: 100, b: 0, br: 128, ef: 0 });
 *
 * EVENTOS:
 *   ble.on('connected',    () => {})
 *   ble.on('disconnected', () => {})
 *   ble.on('led1',         (state) => {})       // bool
 *   ble.on('strip1',       (state) => {})       // { s, r, g, b, br, ef }
 *   ble.on('strip2',       (state) => {})
 *   ble.on('lux',          (state) => {})       // { dark, auto }
 *   ble.on('node',         (state) => {})       // { enabled, online, dark, motion, pir_enabled, hold_ms }
 *   ble.on('pir',          (state) => {})       // { enabled, motion, keep_on }
 *   ble.on('netStatus',    (state) => {})       // { wc, ws, mc, mb }
 *   ble.on('wifiResult',   (ok) => {})          // bool
 *   ble.on('mqttResult',   (ok) => {})          // bool
 */

// UUIDs — deben coincidir exactamente con ble_protocol.h
const UUIDS = {
  SVC_DEVICE_CONTROL:  '12345678-1234-1234-1234-0000aa010000',
  SVC_SENSOR_STATUS:   '12345678-1234-1234-1234-0000aa020000',
  SVC_NETWORK_CONFIG:  '12345678-1234-1234-1234-0000aa030000',
  SVC_DEVICE_INFO:     '0000180a-0000-1000-8000-00805f9b34fb',

  // Device Control
  CHAR_LED1:    '12345678-1234-1234-1234-0001aa010000',
  CHAR_STRIP1:  '12345678-1234-1234-1234-0002aa010000',
  CHAR_STRIP2:  '12345678-1234-1234-1234-0003aa010000',

  // Sensor Status
  CHAR_LUX:    '12345678-1234-1234-1234-0001aa020000',
  CHAR_NODE:   '12345678-1234-1234-1234-0002aa020000',
  CHAR_PIR:    '12345678-1234-1234-1234-0003aa020000',

  // Network Config
  CHAR_WIFI:       '12345678-1234-1234-1234-0001aa030000',
  CHAR_MQTT:       '12345678-1234-1234-1234-0002aa030000',
  CHAR_NET_STATUS: '12345678-1234-1234-1234-0003aa030000',
};

export class EspHomeBLE extends EventTarget {
  constructor() {
    super();
    this._device    = null;
    this._server    = null;
    this._chars     = {};       // uuid → BluetoothRemoteGATTCharacteristic
    this._state     = {
      led1:      false,
      strip1:    { s: false, r: 255, g: 100, b: 0, br: 128, ef: 0 },
      strip2:    { s: false, r: 255, g: 200, b: 100, br: 200, ef: 0 },
      lux:       { dark: false, auto: false },
      node:      { enabled: true, online: false, dark: false, motion: false, pir_enabled: true, hold_ms: 0 },
      pir:       { enabled: true, motion: false, keep_on: false },
      netStatus: { wc: false, ws: '', mc: false, mb: '' },
    };
  }

  // ── Conexión ────────────────────────────────────────────────────────────────

  async connect() {
    if (!navigator.bluetooth) {
      throw new Error('Web Bluetooth no está disponible. Usa Chrome/Edge con HTTPS.');
    }

    this._device = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'ESP-HOME' }],
      optionalServices: [
        UUIDS.SVC_DEVICE_CONTROL,
        UUIDS.SVC_SENSOR_STATUS,
        UUIDS.SVC_NETWORK_CONFIG,
        UUIDS.SVC_DEVICE_INFO,
      ],
    });

    this._device.addEventListener('gattserverdisconnected', () => this._onDisconnected());
    this._server = await this._device.gatt.connect();

    await this._discoverAll();
    await this._subscribeAll();
    await this._readAll();         // leer estado inicial completo

    this._emit('connected');
    return this._state;
  }

  async disconnect() {
    if (this._device?.gatt?.connected) {
      this._device.gatt.disconnect();
    }
  }

  get connected() {
    return this._device?.gatt?.connected ?? false;
  }

  get state() {
    return this._state;
  }

  // ── Discovery y suscripción ─────────────────────────────────────────────────

  async _discoverAll() {
    const discoverChars = async (svcUuid, charUuids) => {
      const svc = await this._server.getPrimaryService(svcUuid);
      for (const uuid of charUuids) {
        try {
          this._chars[uuid] = await svc.getCharacteristic(uuid);
        } catch (e) {
          console.warn(`Característica ${uuid} no encontrada:`, e.message);
        }
      }
    };

    await discoverChars(UUIDS.SVC_DEVICE_CONTROL, [
      UUIDS.CHAR_LED1, UUIDS.CHAR_STRIP1, UUIDS.CHAR_STRIP2
    ]);
    await discoverChars(UUIDS.SVC_SENSOR_STATUS, [
      UUIDS.CHAR_LUX, UUIDS.CHAR_NODE, UUIDS.CHAR_PIR
    ]);
    await discoverChars(UUIDS.SVC_NETWORK_CONFIG, [
      UUIDS.CHAR_WIFI, UUIDS.CHAR_MQTT, UUIDS.CHAR_NET_STATUS
    ]);
  }

  async _subscribeAll() {
    const subscribe = async (uuid, handler) => {
      const char = this._chars[uuid];
      if (!char?.properties.notify) return;
      await char.startNotifications();
      char.addEventListener('characteristicvaluechanged', (e) => handler(e.target.value));
    };

    await subscribe(UUIDS.CHAR_LED1,       (v) => this._onLed1Notify(v));
    await subscribe(UUIDS.CHAR_STRIP1,     (v) => this._onStrip1Notify(v));
    await subscribe(UUIDS.CHAR_STRIP2,     (v) => this._onStrip2Notify(v));
    await subscribe(UUIDS.CHAR_LUX,        (v) => this._onLuxNotify(v));
    await subscribe(UUIDS.CHAR_NODE,       (v) => this._onNodeNotify(v));
    await subscribe(UUIDS.CHAR_PIR,        (v) => this._onPirNotify(v));
    await subscribe(UUIDS.CHAR_WIFI,       (v) => this._onWifiResultNotify(v));
    await subscribe(UUIDS.CHAR_MQTT,       (v) => this._onMqttResultNotify(v));
    await subscribe(UUIDS.CHAR_NET_STATUS, (v) => this._onNetStatusNotify(v));
  }

  async _readAll() {
    const readAndParse = async (uuid, handler) => {
      const char = this._chars[uuid];
      if (!char?.properties.read) return;
      try {
        const value = await char.readValue();
        handler(value);
      } catch (e) {
        console.warn(`Error leyendo ${uuid}:`, e.message);
      }
    };

    await readAndParse(UUIDS.CHAR_LED1,       (v) => this._onLed1Notify(v));
    await readAndParse(UUIDS.CHAR_STRIP1,     (v) => this._onStrip1Notify(v));
    await readAndParse(UUIDS.CHAR_STRIP2,     (v) => this._onStrip2Notify(v));
    await readAndParse(UUIDS.CHAR_LUX,        (v) => this._onLuxNotify(v));
    await readAndParse(UUIDS.CHAR_NODE,       (v) => this._onNodeNotify(v));
    await readAndParse(UUIDS.CHAR_PIR,        (v) => this._onPirNotify(v));
    await readAndParse(UUIDS.CHAR_NET_STATUS, (v) => this._onNetStatusNotify(v));
  }

  // ── Parsers de notify ───────────────────────────────────────────────────────

  _onLed1Notify(view) {
    this._state.led1 = view.getUint8(0) !== 0;
    this._emit('led1', this._state.led1);
  }

  _onStrip1Notify(view) {
    this._state.strip1 = this._parseStripJson(view);
    this._emit('strip1', this._state.strip1);
  }

  _onStrip2Notify(view) {
    this._state.strip2 = this._parseStripJson(view);
    this._emit('strip2', this._state.strip2);
  }

  _parseStripJson(view) {
    try {
      const text = new TextDecoder().decode(view.buffer);
      return JSON.parse(text);
    } catch {
      return this._state.strip1; // fallback estado actual
    }
  }

  _onLuxNotify(view) {
    const byte = view.getUint8(0);
    this._state.lux = {
      dark: (byte & 0x01) !== 0,
      auto: (byte & 0x02) !== 0,
    };
    this._emit('lux', this._state.lux);
  }

  _onNodeNotify(view) {
    // 7 bytes: [enabled][online][dark][motion][pir_enabled][hold_lo][hold_hi]
    this._state.node = {
      enabled:     view.getUint8(0) !== 0,
      online:      view.getUint8(1) !== 0,
      dark:        view.getUint8(2) !== 0,
      motion:      view.getUint8(3) !== 0,
      pir_enabled: view.getUint8(4) !== 0,
      hold_ms:     view.getUint8(5) | (view.getUint8(6) << 8),
    };
    this._emit('node', this._state.node);
  }

  _onPirNotify(view) {
    const byte = view.getUint8(0);
    this._state.pir = {
      enabled:  (byte & 0x01) !== 0,
      motion:   (byte & 0x02) !== 0,
      keep_on:  (byte & 0x04) !== 0,
    };
    this._emit('pir', this._state.pir);
  }

  _onWifiResultNotify(view) {
    const ok = view.getUint8(0) === 0x01;
    this._emit('wifiResult', ok);
  }

  _onMqttResultNotify(view) {
    const ok = view.getUint8(0) === 0x01;
    this._emit('mqttResult', ok);
  }

  _onNetStatusNotify(view) {
    try {
      const text = new TextDecoder().decode(view.buffer);
      this._state.netStatus = JSON.parse(text);
      this._emit('netStatus', this._state.netStatus);
    } catch (e) {
      console.warn('Error parseando netStatus:', e);
    }
  }

  _onDisconnected() {
    this._chars = {};
    this._emit('disconnected');
    // Auto-reconexión tras 2s
    setTimeout(() => this._tryReconnect(), 2000);
  }

  async _tryReconnect() {
    if (!this._device) return;
    try {
      console.log('Intentando reconexión BLE...');
      this._server = await this._device.gatt.connect();
      await this._discoverAll();
      await this._subscribeAll();
      await this._readAll();
      this._emit('connected');
    } catch (e) {
      console.warn('Reconexión fallida, reintentando en 3s...', e.message);
      setTimeout(() => this._tryReconnect(), 3000);
    }
  }

  // ── API de comandos — equivalentes a los endpoints HTTP ────────────────────

  /** Equivalente a GET /toggle */
  async toggleLED() {
    return this._write(UUIDS.CHAR_LED1, new Uint8Array([0x02])); // CMD_TOGGLE
  }

  /** Equivalente a GET /status (el estado ya está en this.state.led1) */
  async getLEDState() {
    return this._state.led1;
  }

  /** Equivalente a POST /strip1/set o /strip2/set
   *  @param {1|2} stripNum
   *  @param {{ s?, r?, g?, b?, br?, ef? }} config
   */
  async setStrip(stripNum, config) {
    const uuid = stripNum === 1 ? UUIDS.CHAR_STRIP1 : UUIDS.CHAR_STRIP2;
    const current = stripNum === 1 ? this._state.strip1 : this._state.strip2;
    const merged = { ...current, ...config };
    const json = JSON.stringify(merged);
    return this._write(uuid, new TextEncoder().encode(json));
  }

  /** Equivalente a GET /lux/auto (toggle) */
  async toggleAutoMode() {
    return this._write(UUIDS.CHAR_LUX, new Uint8Array([0x02])); // BIT1 = toggle auto
  }

  /** Equivalente a GET /node/toggle */
  async toggleNode() {
    return this._write(UUIDS.CHAR_NODE, new Uint8Array([0x01]));
  }

  /** Equivalente a GET /pir/toggle */
  async togglePIR() {
    return this._write(UUIDS.CHAR_PIR, new Uint8Array([0x01]));
  }

  /**
   * Equivalente a POST /config/wifi
   * @param {string} ssid
   * @param {string} password
   * @returns {Promise<boolean>} resultado vía notify (wifiResult event)
   */
  async setWiFi(ssid, password) {
    // Formato: SSID\0PASSWORD
    const encoder = new TextEncoder();
    const ssidBytes = encoder.encode(ssid);
    const passBytes = encoder.encode(password);
    const buf = new Uint8Array(ssidBytes.length + 1 + passBytes.length);
    buf.set(ssidBytes, 0);
    buf[ssidBytes.length] = 0x00;       // separador null
    buf.set(passBytes, ssidBytes.length + 1);
    return this._write(UUIDS.CHAR_WIFI, buf);
    // El resultado llega por notify 'wifiResult'
  }

  /** Equivalente a GET /config/wifi/clear */
  async clearWiFi() {
    return this._write(UUIDS.CHAR_WIFI, new Uint8Array([0xFF]));
  }

  /**
   * Equivalente a POST /config/mqtt
   * @param {string} brokerUrl  e.g. "mqtt://192.168.1.100:1883"
   */
  async setMQTT(brokerUrl) {
    return this._write(UUIDS.CHAR_MQTT, new TextEncoder().encode(brokerUrl));
    // El resultado llega por notify 'mqttResult'
  }

  /** Equivalente a GET /config/mqtt/clear */
  async clearMQTT() {
    return this._write(UUIDS.CHAR_MQTT, new Uint8Array([0xFF]));
  }

  // ── Helpers internos ────────────────────────────────────────────────────────

  async _write(uuid, data) {
    const char = this._chars[uuid];
    if (!char) throw new Error(`Característica ${uuid} no disponible`);
    await char.writeValueWithResponse(data);
  }

  /** Helper de eventos: on('led1', callback) */
  on(event, callback) {
    this.addEventListener(event, (e) => callback(e.detail));
    return this; // chainable
  }

  _emit(event, detail) {
    this.dispatchEvent(new CustomEvent(event, { detail }));
  }
}
