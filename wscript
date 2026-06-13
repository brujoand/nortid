#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import json
import os
import os.path
import re
import subprocess

top = '.'
out = 'build'


def _sync_version_from_git():
    """Stamp package.json's version from the latest git tag so the built .pbw
    (and on-watch versionLabel) always reflect the real release version
    instead of a stale hand-edited value. No-op when git/tags are unavailable.
    Rewrites only the version string so the rest of the file (formatting,
    compact arrays) is left untouched."""
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        tag = subprocess.check_output(
            ['git', 'describe', '--tags', '--abbrev=0'],
            stderr=subprocess.DEVNULL,
            cwd=here,
        ).decode().strip()
    except (subprocess.CalledProcessError, OSError):
        return
    version = tag.lstrip('v')
    if not version:
        return
    path = os.path.join(here, 'package.json')
    with open(path) as f:
        text = f.read()
    if json.loads(text).get('version') == version:
        return
    new_text, count = re.subn(
        r'("version"\s*:\s*")[^"]*(")',
        lambda m: m.group(1) + version + m.group(2),
        text,
        count=1,
    )
    if count == 1:
        with open(path, 'w') as f:
            f.write(new_text)


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    _sync_version_from_git()
    ctx.load('pebble_sdk')


def build(ctx):
    _sync_version_from_git()
    ctx.load('pebble_sdk')

    binaries = []

    # Screenshot/demo builds set NORTID_SCREENSHOT_DEMO=1 so the watchface shows
    # representative health values instead of "--" off-watch. Never set for the
    # released .pbw.
    demo = os.environ.get('NORTID_SCREENSHOT_DEMO') == '1'

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.set_group(ctx.env.PLATFORM_NAME)
        if demo:
            ctx.env.append_value('DEFINES', 'SCREENSHOT_DEMO=1')
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')
