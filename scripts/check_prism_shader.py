"""Exercise the actual Atmosphere GLSL on an EGL context (pip install moderngl numpy).
This is shader QA, not a gameplay FPS test.
"""
import argparse
import json
from pathlib import Path

import moderngl
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    ctx = moderngl.create_standalone_context(backend='egl', require=330)
    program = ctx.program(
        vertex_shader='#version 330 core\n' + (root / 'data/shader/prism_atmosphere.vert').read_text(),
        fragment_shader='#version 330 core\n' + (root / 'data/shader/prism_atmosphere.frag').read_text())
    vao = ctx.vertex_array(program, [])
    neutral = {'gGrade': (0, 1, 1, 1), 'gDepthBloom': (0, 0, 0, 0.8),
               'gSpreadVignette': (4, 0, 0, 0), 'gTint': (1, 1, 1, 0)}
    checks = []
    rng = np.random.default_rng(20260930)
    for width, height in [(1, 1), (65, 37), (320, 180), (800, 600)]:
        source = rng.random((height, width, 4), dtype=np.float32)
        texture = ctx.texture((width, height), 4, source.tobytes(), dtype='f4')
        texture.filter = (moderngl.LINEAR, moderngl.LINEAR)
        texture.repeat_x = texture.repeat_y = False
        target = ctx.texture((width, height), 4, dtype='f4')
        fbo = ctx.framebuffer([target])
        fbo.use()
        texture.use(0)
        program['gTextureSampler'] = 0

        def render(changes=None):
            for key, value in (neutral | (changes or {})).items():
                program[key] = value
            vao.render(vertices=3)
            return np.frombuffer(fbo.read(components=4, dtype='f4'), np.float32).reshape(source.shape)

        result = render()
        error = float(np.abs(source - result).max())
        assert error < 0.0002, ('neutral changed pixels/orientation', error)
        saturated = render({'gGrade': (0, 1, 0, 1)})
        assert np.max(np.abs(saturated[:, :, 0] - saturated[:, :, 1])) < 0.0001
        assert np.max(np.abs(saturated[:, :, 0] - saturated[:, :, 2])) < 0.0001
        # Both safe extrema and very narrow gamma: no NaN/Inf or out-of-range output.
        for settings in [
            {'gGrade': (-4, 4, 4, 0.25), 'gDepthBloom': (-1, -1, 5, 0),
             'gSpreadVignette': (32, 1, 0, 0), 'gTint': (0.2, 0.5, 1, 1)},
            {'gGrade': (4, 0, 0, 4), 'gDepthBloom': (1, 1, 5, 0)},
            {'gDepthBloom': (0, 0, 5, 1)},
        ]:
            result = render(settings)
            assert np.isfinite(result).all()
            assert result.min() >= 0 and result.max() <= 1
            assert np.max(np.abs(result[:, :, 3] - source[:, :, 3])) < 0.0002
        checks.append({'size': [width, height], 'neutral_max_error': error})
        fbo.release()
        target.release()
        texture.release()
    report = {'renderer': ctx.info['GL_RENDERER'], 'version': ctx.info['GL_VERSION'],
              'scope': 'isolated GLSL correctness; no DDNet/manual gameplay validation',
              'checks': checks}
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
