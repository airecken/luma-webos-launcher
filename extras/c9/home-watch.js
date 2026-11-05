/* C9 Home overlay routing. ES5 for the TV's Node 0.12.2. */
'use strict';
var HOME = ['com.webos.app.home', 'com.webos.app.fullhome'];
var LAUNCHER = 'com.denisefremov.luma';
var FLAG = '/tmp/lumen-c9-native-home';
var GALLERY = 'com.ammar027.lumen.gallery.c9test';
var SAVER = 'com.webos.app.screensaver';
function createRouter(deps) {
  var showing = false,
    bypass = false,
    pending = false,
    generation = 0;
  return function (reply) {
    if (!reply || !Array.isArray(reply.foregroundAppInfo)) return;
    var homes = reply.foregroundAppInfo.filter(function (w) {
      return HOME.indexOf(w.appId) >= 0;
    });
    if (!homes.length) {
      showing = false;
      bypass = false;
      generation++;
      return;
    }
    if (showing || pending) return;
    showing = true;
    bypass = deps.consumeBypass();
    if (bypass || !deps.installed()) return;
    var thisGeneration = generation;
    pending = true;
    deps.launch(LAUNCHER, function (ok) {
      pending = false;
      if (!ok) {
        deps.log('Luma launch failed; LG Home stays available');
        return;
      }
      if (thisGeneration === generation)
        homes.forEach(function (h) {
          deps.close(h.appId);
        });
      deps.log('Home opened Luma');
    });
  };
}
function createSaverRouter(deps) {
  var showing = false,
    generation = 0;
  return function (reply) {
    if (!reply || !Array.isArray(reply.foregroundAppInfo)) return;
    var windows = reply.foregroundAppInfo;
    var nativeSaver = windows.some(function (w) {
      return w.appId === SAVER;
    });
    if (!nativeSaver) {
      showing = false;
      generation++;
      return;
    }
    if (showing) return;
    showing = true;
    if (!deps.enabled() || !deps.installed()) return;
    var cards = windows.filter(function (w) {
      return w.windowType === '_WEBOS_WINDOW_TYPE_CARD';
    });
    var previous = cards.length ? cards[cards.length - 1].appId : LAUNCHER;
    if (previous === GALLERY) {
      deps.closeNative();
      deps.log('Kept Gallery visible over the native idle saver');
      return;
    }
    var thisGeneration = generation;
    deps.launch(
      { id: GALLERY, params: { mode: 'screensaver', returnApp: previous } },
      function (ok) {
        if (!ok) {
          deps.log('Gallery launch failed; native screensaver retained');
          return;
        }
        if (thisGeneration === generation) deps.closeNative();
        deps.log('Gallery screensaver opened; return app: ' + previous);
      }
    );
  };
}
module.exports = { createRouter: createRouter, createSaverRouter: createSaverRouter };
function runWatcher() {
  var fs = require('fs'),
    cp = require('child_process'),
    child = null,
    retry = null,
    stopping = false;
  var pidFile = '/tmp/lumen-c9-home.pid';
  fs.writeFileSync(pidFile, String(process.pid));
  function log(message) {
    console.log(new Date().toISOString() + ' ' + message);
  }
  function call(method, params, done) {
    cp.execFile(
      '/usr/bin/luna-send',
      [
        '-n',
        '1',
        '-w',
        '3000',
        'luna://com.webos.applicationManager/' + method,
        JSON.stringify(params)
      ],
      { timeout: 4000 },
      function (err, stdout) {
        var ok = false;
        try {
          ok = !err && JSON.parse(stdout).returnValue === true;
        } catch (ignore) {
          void ignore;
        }
        if (done) done(ok);
      }
    );
  }
  var route = createRouter({
    consumeBypass: function () {
      try {
        var age = Date.now() - fs.statSync(FLAG).mtime.getTime();
        fs.unlinkSync(FLAG);
        return age >= 0 && age < 8000;
      } catch (ignore) {
        void ignore;
        return false;
      }
    },
    installed: function () {
      return fs.existsSync(
        '/media/developer/apps/usr/palm/applications/' + LAUNCHER + '/appinfo.json'
      );
    },
    launch: function (id, cb) {
      call('launch', { id: id }, cb);
    },
    close: function (id) {
      call('closeByAppId', { id: id });
    },
    log: log
  });
  var routeSaver = createSaverRouter({
    enabled: function () {
      return fs.existsSync('/var/lib/webosbrew/lumen-c9-home/gallery-screensaver');
    },
    installed: function () {
      return fs.existsSync(
        '/media/developer/apps/usr/palm/applications/' + GALLERY + '/appinfo.json'
      );
    },
    launch: function (params, cb) {
      call('launch', params, cb);
    },
    closeNative: function () {
      call('closeByAppId', { id: SAVER });
    },
    log: log
  });
  function connect() {
    if (stopping) return;
    var buffer = '';
    child = cp.spawn('/usr/bin/luna-send', [
      '-i',
      'luna://com.webos.applicationManager/getForegroundAppInfo',
      '{"subscribe":true,"extraInfo":true}'
    ]);
    child.stdout.on('data', function (chunk) {
      buffer += String(chunk);
      var lines = buffer.split(/\r?\n/);
      buffer = lines.pop();
      lines.forEach(function (line) {
        try {
          var reply = JSON.parse(line);
          route(reply);
          routeSaver(reply);
        } catch (err) {
          log(String(err));
        }
      });
      if (buffer.length > 65536) buffer = '';
    });
    child.stderr.on('data', function () {});
    child.on('error', function (err) {
      log(String(err));
    });
    child.on('close', function () {
      child = null;
      if (!stopping) retry = setTimeout(connect, 3000);
    });
  }
  function stop() {
    stopping = true;
    if (retry) clearTimeout(retry);
    if (child) child.kill();
    try {
      if (fs.readFileSync(pidFile, 'utf8') === String(process.pid)) fs.unlinkSync(pidFile);
    } catch (ignore) {
      void ignore;
    }
    setTimeout(function () {
      process.exit(0);
    }, 100);
  }
  process.on('SIGTERM', stop);
  process.on('SIGINT', stop);
  log('C9 Home router started');
  connect();
}

if (require.main === module) runWatcher();
