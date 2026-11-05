import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const source = await fs.readFile(
  new URL('../extras/wallpaper/app/wallpaper.js', import.meta.url),
  'utf8',
);

function player(params) {
  const mediaEvents = {},
    events = {},
    timers = new Map(),
    launches = [];
  let timerId = 0,
    closed = 0;
  const video = {
    paused: true,
    currentTime: 0,
    readyState: 4,
    error: null,
    addEventListener(name, callback) {
      mediaEvents[name] = callback;
    },
    pause() {
      this.paused = true;
    },
    load() {},
    play() {
      this.paused = false;
      return Promise.resolve();
    },
  };
  const document = {
    hidden: false,
    getElementById(id) {
      return id === 'video' ? video : { textContent: '' };
    },
    addEventListener(name, callback) {
      events[name] = callback;
    },
  };
  const sandbox = {
    document,
    PalmSystem: { launchParams: JSON.stringify(params) },
    PalmServiceBridge: function () {
      this.call = (uri, body) => {
        launches.push({ uri, body: JSON.parse(body) });
        this.onservicecallback('{"returnValue":true}');
      };
    },
    close() {
      closed++;
    },
    setTimeout(callback) {
      timers.set(++timerId, callback);
      return timerId;
    },
    clearTimeout(id) {
      timers.delete(id);
    },
  };
  sandbox.window = sandbox;
  vm.runInNewContext(source, sandbox);
  return {
    video,
    document,
    events,
    mediaEvents,
    launches,
    closed: () => closed,
    flush() {
      const pending = [...timers.values()];
      timers.clear();
      pending.forEach((callback) => callback());
    },
  };
}

test('selected clips advance without a repeat and playback stays muted', () => {
  const p = player({
    clips: [{ url: 'https://example.com/a.mp4' }, { url: 'https://example.com/b.mp4' }],
    index: 1,
    shuffle: true,
  });
  assert.equal(p.video.src, 'https://example.com/b.mp4');
  assert.equal(p.video.paused, true);
  p.flush();
  assert.equal(p.video.paused, false);
  assert.equal(p.video.muted, true);
  assert.equal(p.video.loop, false);
  p.mediaEvents.ended();
  assert.equal(p.video.src, 'https://example.com/a.mp4');
});

test('one clip loops and a remote key returns only to Luma', () => {
  const p = player({ url: 'https://example.com/a.mp4' });
  p.flush();
  assert.equal(p.video.loop, true);
  p.events.keydown({ preventDefault() {} });
  p.events.keydown({ preventDefault() {} });
  assert.equal(p.video.paused, true);
  assert.deepEqual(p.launches, [
    { uri: 'luna://com.webos.applicationManager/launch', body: { id: 'com.denisefremov.luma' } },
  ]);
  assert.equal(p.closed(), 1);
});

test('invalid URLs are discarded and a playback error returns Home', () => {
  const p = player({
    clips: [
      { url: 'http://example.com/no.mp4' },
      { url: 'https://example.com/yes.mp4' },
      { url: 'https://example.com/\nbad.mp4' },
    ],
    index: 999,
  });
  assert.equal(p.video.src, 'https://example.com/yes.mp4');
  p.flush();
  p.mediaEvents.error();
  p.flush();
  assert.equal(p.launches.length, 1);
  assert.equal(p.closed(), 1);
});

test('hiding the wallpaper cancels pending playback', () => {
  const p = player({ url: 'https://example.com/a.mp4' });
  p.document.hidden = true;
  p.events.visibilitychange();
  p.flush();
  assert.equal(p.video.paused, true);
  assert.equal(p.launches.length, 0);
});
