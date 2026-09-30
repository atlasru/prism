"""Measure six visual profiles against the same local demo using DDNet's frame timer.
Usage: python scripts/benchmark_prism_visuals.py path/to/Prism.exe path/to/scene.demo
Runs isolated portable profiles; never edits the user's saved settings.
"""
import argparse
import json
from pathlib import Path
import re
import statistics
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client', type=Path)
    parser.add_argument('demo', type=Path)
    parser.add_argument('--seconds', type=int, default=25)
    parser.add_argument('--warmup', type=float, default=5)
    parser.add_argument('--output', type=Path, default=Path('prism-visual-benchmark.json'))
    args = parser.parse_args()
    client, demo = args.client.resolve(), args.demo.resolve()
    if not client.is_file() or not demo.is_file():
        parser.error('Client and demo must exist.')
    if args.seconds < 10 or args.seconds <= args.warmup:
        parser.error('Use at least 10 seconds and a shorter warmup.')
    header = demo.read_bytes()[:176]
    if len(header) < 176 or header[:7] != b'TWDEMO\x00':
        parser.error('Invalid DDNet demo header.')
    duration = int.from_bytes(header[152:156], 'big')
    if duration < args.seconds + 1:
        parser.error('Demo must be longer than the benchmark duration.')
    profiles = {
        'baseline': [],
        'glow': ['prism_player_glow 1'],
        'atmosphere': ['prism_apply_atmosphere_preset 1', 'prism_bloom_strength 0'],
        'bloom': ['prism_apply_atmosphere_preset 1', 'prism_bloom_strength 100'],
        'glow_atmosphere': ['prism_player_glow 1', 'prism_apply_atmosphere_preset 1'],
        'aggressive': ['prism_player_glow 1', 'prism_glow_radius 192',
                       'prism_glow_intensity 400', 'prism_glow_alpha 100',
                       'prism_apply_atmosphere_preset 1', 'prism_bloom_strength 500',
                       'prism_bloom_threshold 0', 'prism_bloom_radius 32'],
    }
    report = {'client': str(client), 'demo': str(demo), 'profiles': {}}
    for name, commands in profiles.items():
        with tempfile.TemporaryDirectory(prefix='prism-benchmark-') as directory:
            cwd = Path(directory)
            (cwd / 'storage.cfg').write_text(
                f'add_path {cwd.as_posix()}\nadd_path {client.parent.as_posix()}\n')
            framefile = cwd / 'frames.txt'
            base = ['gfx_backend opengl', 'gfx_gl_major 3', 'gfx_gl_minor 3',
                    'gfx_fullscreen 0', 'gfx_screen_width 1280', 'gfx_screen_height 720',
                    'gfx_vsync 0', 'gfx_refresh_rate 0', 'snd_enable 0',
                    'cl_show_welcome 0', 'cl_showfps 0', 'prism_enabled 1',
                    'stdout_output_level 1',
                    'prism_glow_others 1', 'prism_glow_local 1', 'prism_glow_dummy 1']
            process = subprocess.run(
                [str(client), *base, *commands, f'play "{demo.as_posix()}"',
                 f'benchmark_quit {args.seconds} "{framefile.as_posix()}"'],
                cwd=cwd, capture_output=True, text=True, errors='replace',
                timeout=args.seconds + 120)
            if process.returncode != 0 or not framefile.exists():
                raise RuntimeError(f'{name}: client failed\n{process.stdout}\n{process.stderr}')
            logs = process.stdout + process.stderr
            if 'loaded map ' not in logs or 'Stopped playback due to error' in logs:
                raise RuntimeError(f'{name}: demo/map did not load cleanly\n{logs}')
            if 'Atmosphere shader unavailable'  in process.stdout + process.stderr:
                raise RuntimeError('Atmosphere shader unavailable; results would be misleading.')
            settings = (cwd / 'settings_prism.cfg').read_text()
            major = re.search(r'^gfx_gl_major (\d+)$', settings, re.M)
            minor = re.search(r'^gfx_gl_minor (\d+)$', settings, re.M)
            # Default values are omitted; the requested 3.3 version is the default.
            version = (int(major[1]) if major else 3, int(minor[1]) if minor else 3)
            if version < (3, 3):
                raise RuntimeError(f'OpenGL fallback {version}: cannot benchmark Atmosphere.')
            frames = [int(value) for value in re.findall(r'^Frametime (\d+) us$',
                                                        framefile.read_text(), re.M)]
            if not frames:
                raise RuntimeError(f'{name}: no frame measurements')
            elapsed = 0
            retained = []
            for frame in frames:
                elapsed += frame / 1_000_000
                if elapsed >= args.warmup and frame > 0:
                    retained.append(frame)
            if not retained:
                raise RuntimeError(f'{name}: warmup consumed all samples')
            average = statistics.mean(retained) / 1000
            report['profiles'][name] = {
                'average_fps': 1000 / average,
                'average_frame_ms': average,
                'p95_frame_ms': sorted(retained)[int((len(retained) - 1) * 0.95)] / 1000,
                'frames': len(retained),
            }
    baseline = report['profiles']['baseline']['average_frame_ms']
    for profile in report['profiles'].values():
        profile['relative_frame_overhead_percent'] = (
            profile['average_frame_ms'] / baseline - 1) * 100
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
