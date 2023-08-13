// Define the cache version
const CACHE_VERSION = '66a40123b59f38c1e7c1b19a4d4da645186189f3';

// Define an array of URLs to cache
const CACHE_URLS = [
  '/',  	
  'SkyEmu.js',
  'SkyEmu.wasm',
  'android-chrome-192x192.png',
  'android-chrome-512x512.png',
  'apple-touch-icon.png',
  'browserconfig.xml',
  'favicon-16x16.png',
  'favicon-32x32.png',
  'favicon.ico',
  'html_code.html',
  'index.html',
  'mstile-150x150.png',
  'safari-pinned-tab.svg',
  'site.webmanifest'
];

// Install the service worker and cache all URLs
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(CACHE_VERSION)
      .then(cache => cache.addAll(CACHE_URLS))
      .then(() => self.skipWaiting())
  );
});

// Activate the service worker and delete old caches
self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(
        keys.filter(key => key !== CACHE_VERSION)
          .map(key => caches.delete(key))
      ))
      .then(() => self.clients.claim())
  );
});

// Fetch requests from the cache first, then the network
self.addEventListener('fetch', event => {
  event.respondWith(
    caches.match(event.request)
      .then(response => response || fetch(event.request))
  );
});
