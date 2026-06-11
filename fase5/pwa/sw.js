// sw.js — Service Worker para ESP-HOME PWA
// Permite instalar la app y usarla offline (BLE no requiere internet)

const CACHE_NAME = 'esp-home-v1';
const ASSETS = [
  '/',
  '/index.html',
  '/ble-client.js',
  '/manifest.json',
];

self.addEventListener('install', (e) => {
  e.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(ASSETS))
  );
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys().then(keys =>
      Promise.all(keys.filter(k => k !== CACHE_NAME).map(k => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', (e) => {
  // Solo cachear recursos de la propia PWA, no llamadas BLE
  if (e.request.url.startsWith(self.location.origin)) {
    e.respondWith(
      caches.match(e.request).then(cached => cached || fetch(e.request))
    );
  }
});
