import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

test('C9 clocked backend preserves presentation timestamps and cleans up its video plane', async () => {
  const root = path.resolve(import.meta.dirname, '..');
  const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'luma-clocked-'));
  try {
    // Only the platform declarations are needed; all real calls are faked by the fixture.
    const header = await fs.readFile(path.join(root, 'tests/native/AcbAPI.h'), 'utf8');
    await fs.writeFile(path.join(dir, 'AcbAPI.h'), header);
    const sources = [
      'tests/native/c9-clocked.c',
      'native/smp/luma_smp_c9.c',
      'native/smp/vendor/smp_player.c',
      'native/smp/vendor/smp_payload.c',
      'native/smp/vendor/strbuf.c',
      'native/smp/vendor/pacer.c',
    ];
    const binary = path.join(dir, 'check');
    const compiled = spawnSync(
      process.env.CC || 'cc',
      [
        '-std=gnu11',
        '-pthread',
        '-I',
        dir,
        '-I',
        path.join(root, 'native/smp/vendor'),
        ...sources.map((s) => path.join(root, s)),
        '-o',
        binary,
      ],
      { encoding: 'utf8' },
    );
    assert.equal(compiled.status, 0, compiled.stderr);
    const checked = spawnSync(binary, [], { encoding: 'utf8' });
    assert.equal(checked.status, 0, checked.stderr);
  } finally {
    await fs.rm(dir, { recursive: true, force: true });
  }
});
