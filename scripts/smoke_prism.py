"""Run the packaged Windows client; test saved settings across real process restarts.
No visual observation or gameplay benchmark is implied by these checks.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

package = Path(sys.argv[1]).resolve()
results = []
with tempfile.TemporaryDirectory(prefix='prism-smoke-') as directory:
    cwd = Path(directory)
    (cwd / 'storage.cfg').write_text(f'add_path {cwd.as_posix()}\nadd_path {package.as_posix()}\n')
    def run(commands):
        args = [str(package / 'Prism.exe'), 'gfx_backend opengl', 'gfx_gl_major 1',
                'gfx_gl_minor 4', 'gfx_fullscreen 0', 'gfx_screen_width 800',
                'gfx_screen_height 600', 'snd_enable 0', 'cl_show_welcome 0', *commands, 'quit']
        result = subprocess.run(args, cwd=cwd, capture_output=True, text=True, errors='replace', timeout=90)
        results.append({'commands': commands, 'exit_code': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        assert result.returncode == 0, results[-1]
        config = cwd / 'settings_prism.cfg'
        assert config.exists(), 'Client did not save settings; startup may have failed'
        values = {}
        for line in config.read_text().splitlines():
            if line.startswith('prism_'):
                key, value = line.split(' ', 1)
                values[key] = value
        return values
    values = run(['prism_apply_preset 3', 'prism_local_glow_intensity 37', 'prism_overlay 1'])
    assert values.get('prism_local_glow_intensity') == '37', values
    values = run([])
    assert values.get('prism_local_glow_intensity') == '37', values
    assert values.get('prism_overlay') == '1', values
    values = run(['prism_other_outline_width -900', 'prism_local_glow_intensity 900'])
    assert values.get('prism_other_outline_width') == '1', values
    assert values.get('prism_local_glow_intensity') == '100', values
    values = run(['prism_apply_preset 0'])
    assert values.get('prism_enabled', '0') == '0', values
    values = run(['prism_apply_preset 1', 'prism_toggle'])
    assert values.get('prism_enabled', '0') == '0', values
    assert values.get('prism_local_outline') == '1', values
    run(['prism_reset'])
Path('dist/runtime-smoke.json').write_text(json.dumps(results, indent=2) + '\n')
print('Packaged-client startup/config persistence smoke: 6 process runs passed. Visual tests not performed.')
