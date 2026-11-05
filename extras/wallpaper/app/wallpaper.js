/* ES5 for Chromium 53. LG's native video pipeline owns presentation timing. */
'use strict';
var video = document.getElementById('video');
var notice = document.getElementById('status');
var leaving = false;
var clips = [];
var index = 0;
var shuffle = false;
var playTimer = 0;
var errorTimer = 0;
var bridges = [];
function luna(uri, body, done) {
  try {
    var bridge = new PalmServiceBridge();
    bridges.push(bridge);
    bridge.onservicecallback = function (reply) {
      var ok = false;
      try {
        ok = JSON.parse(reply).returnValue === true;
      } catch (ignore) {
        void ignore;
      }
      bridges.splice(bridges.indexOf(bridge), 1);
      if (done) done(ok);
    };
    bridge.call(uri, JSON.stringify(body));
  } catch (error) {
    void error;
    if (done) done(false);
  }
}
function leave() {
  if (leaving) return;
  leaving = true;
  clearTimeout(playTimer);
  clearTimeout(errorTimer);
  video.pause();
  luna(
    'luna://com.webos.applicationManager/launch',
    { id: 'com.denisefremov.luma' },
    function (ok) {
      if (ok) window.close();
      else {
        leaving = false;
        notice.textContent = 'Press Home to return to Luma';
      }
    }
  );
}
function failed() {
  if (leaving || document.hidden) return;
  notice.textContent = 'Background could not play. Returning to Luma…';
  clearTimeout(errorTimer);
  errorTimer = setTimeout(leave, 2500);
}
function openClip(delay) {
  if (!clips.length) return failed();
  clearTimeout(playTimer);
  clearTimeout(errorTimer);
  notice.textContent = '';
  video.pause();
  video.muted = true;
  video.loop = clips.length === 1;
  video.src = clips[index].url;
  video.load();
  playTimer = setTimeout(function () {
    if (leaving || document.hidden) return;
    var result = video.play();
    if (result && result.catch) result.catch(failed);
  }, delay);
}
function start() {
  var params = {};
  try {
    params = JSON.parse((window.PalmSystem && PalmSystem.launchParams) || '{}');
  } catch (ignore) {
    void ignore;
  }
  var supplied = params.clips || [{ url: params.url, title: params.title }];
  clips = [];
  if (Array.isArray(supplied))
    supplied.slice(0, 300).forEach(function (clip) {
      if (
        clip &&
        typeof clip.url === 'string' &&
        clip.url.length <= 2048 &&
        /^https:\/\//.test(clip.url) &&
        !/[\r\n]/.test(clip.url)
      ) {
        clips.push({ url: clip.url, title: String(clip.title || '').slice(0, 200) });
      }
    });
  index =
    typeof params.index === 'number' && params.index >= 0 && params.index < clips.length
      ? Math.floor(params.index)
      : 0;
  shuffle = params.shuffle === true;
  leaving = false;
  openClip(800);
}
window.onerror = function () {
  failed();
};
window.wallpaperState = function () {
  return {
    time: video.currentTime,
    paused: video.paused,
    ready: video.readyState,
    error: video.error && video.error.code,
    index: index,
    clips: clips.length
  };
};
video.addEventListener('error', failed);
video.addEventListener('ended', function () {
  if (leaving || document.hidden || clips.length < 2) return;
  index = shuffle
    ? (index + 1 + Math.floor(Math.random() * (clips.length - 1))) % clips.length
    : (index + 1) % clips.length;
  openClip(0);
});
document.addEventListener('keydown', function (event) {
  event.preventDefault();
  leave();
});
document.addEventListener('click', leave);
document.addEventListener('mousewheel', leave);
document.addEventListener('webOSRelaunch', start);
document.addEventListener('visibilitychange', function () {
  if (document.hidden) {
    clearTimeout(playTimer);
    clearTimeout(errorTimer);
    video.pause();
  }
});
start();
